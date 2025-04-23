/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP connector class, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include "asio/ip/tcp.hpp"
#include "asio/post.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/basic_resolver.hpp"
#include "asio/steady_timer.hpp"

#include <system_error>
#include <vector>
#include <memory>
#include <chrono>
#include <future>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/basic_io_output.hpp"

#include "net_ip/endpoints_resolver.hpp"
#include "net_ip/tcp_connector_timeout.hpp"

// TCP connector has the most complicated states of any of the net entity detail
// objects. The states transition from stopped to resolving addresses to connecting
// to connected, then back to connecting or stopped depending on the transition. The
// closing state is used for errors and shutting down. There is also a timeout state 
// for when a connection is refused. As typical, the shutdown logic is non-trivial.
//
// The states and transitions could be implemented with a more formal state transition
// table, including some nice state transition classes from Boost or elsewhere, but
// for now everything is hard-coded and manually set.

namespace chops {
namespace net {
namespace detail {

class tcp_connector : public std::enable_shared_from_this<tcp_connector> {
public:
  using endpoint_type = asio::ip::tcp::endpoint;

private:
  using resolver_type = chops::net::endpoints_resolver<asio::ip::tcp>;
  using resolver_results = asio::ip::basic_resolver_results<asio::ip::tcp>;
  using endpoints = std::vector<endpoint_type>;
  using endpoints_iter = endpoints::const_iterator;

private:
  enum conn_state { stopped, resolving, connecting, connected, timeout, closing };

private:
  net_entity_common<tcp_io>     m_entity_common;
  asio::ip::tcp::socket         m_socket;
  tcp_io_shared_ptr             m_io_handler;
  resolver_type                 m_resolver;
  endpoints                     m_endpoints;
  asio::steady_timer            m_timer;
  std::string                   m_remote_host;
  std::string                   m_remote_port;
  bool                          m_reconn_on_err;
  tcp_connector_timeout_func    m_timeout_func;
  std::size_t                   m_conn_attempts;
  conn_state                    m_state;

public:
  template <typename Iter>
  tcp_connector(asio::io_context& ioc, 
                Iter beg, Iter end,
                tcp_connector_timeout_func tout_func,
                bool reconn_on_err) :
      m_entity_common(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(beg, end),
      m_timer(ioc),
      m_remote_host(),
      m_remote_port(),
      m_reconn_on_err(reconn_on_err),
      m_timeout_func(tout_func),
      m_conn_attempts(0u),
      m_state(stopped)
    { }

  tcp_connector(asio::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                tcp_connector_timeout_func tout_func,
                bool reconn_on_err) :
      m_entity_common(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(),
      m_timer(ioc),
      m_remote_host(remote_host),
      m_remote_port(remote_port),
      m_reconn_on_err(reconn_on_err),
      m_timeout_func(tout_func),
      m_conn_attempts(0u),
      m_state(stopped)
    { }

private:
  // no copy or assignment semantics for this class
  tcp_connector(const tcp_connector&) = delete;
  tcp_connector(tcp_connector&&) = delete;
  tcp_connector& operator=(const tcp_connector&) = delete;
  tcp_connector& operator=(tcp_connector&&) = delete;

public:

  bool is_started() const noexcept { return m_entity_common.is_started(); }

  template <typename F>
  void visit_socket(F&& f) {
    f(m_socket);
  }

  template <typename F>
  std::size_t visit_io_output(F&& func) {
    auto self = shared_from_this();
    std::promise<std::size_t> prom;
    auto fut = prom.get_future();
    // send to executor for concurrency protection
    asio::post(m_socket.get_executor(), [this, self, &func, p = std::move(prom)] () mutable {
        if (m_io_handler && m_io_handler->is_io_started()) {
          func(basic_io_output<tcp_io>(m_io_handler));
          p.set_value(1u);
        }
        else {
          p.set_value(0u);
        }
      }
    );
    return fut.get();
  }

  template <typename F1, typename F2>
  std::error_code start(F1&& io_state_chg, F2&& err_cb) {
    auto self = shared_from_this();
    return m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_cb),
                                 m_socket.get_executor(),
             [this, self] () { return do_start(); } );
  }

  std::error_code stop() {
    auto self = shared_from_this();
    return m_entity_common.stop(m_socket.get_executor(),
             [this, self] () {
               close(std::make_error_code(net_ip_errc::tcp_connector_stopped));
               return std::error_code();
             }
    );
  }


private:

  void clear_strings() noexcept {
    m_remote_host.clear(); // no longer need the string contents
    m_remote_host.shrink_to_fit();
    m_remote_port.clear();
    m_remote_port.shrink_to_fit();
  }

  std::error_code do_start() {
    // empty endpoints container is the indication that a resolve is needed
    if (m_endpoints.empty()) {
      m_state = resolving;
      m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                    std::make_error_code(net_ip_errc::tcp_connector_resolving_addresses));
      auto self = shared_from_this();
      m_resolver.make_endpoints(false, m_remote_host, m_remote_port,
        [this, self] 
             (std::error_code err, resolver_results res) {
          if (err || m_state != resolving) {
            m_state = stopped;
            close(err);
            return;
          }
          for (const auto& e : res) {
            m_endpoints.push_back(e.endpoint());
          }
          clear_strings();
          start_connect(m_timeout_func);
        }
      );
      return { };
    }
    clear_strings();
    start_connect(m_timeout_func);
    return { };
  }

  void close(const std::error_code& err) {
    if (m_state == closing || m_state == stopped) {
      return; // already shutting down or stopped, bypass closing again
    }
    auto sav_state = m_state;
    m_state = closing;
    m_entity_common.set_stopped(); // for internal closes
    std::error_code ec;
    m_socket.close(ec);
    switch (sav_state) {
      case stopped: {
        break;
      }
      case resolving: {
        m_resolver.cancel();
        break;
      }
      case connecting: {
        // socket close should cancel any connect attempts
        break;
      }
      case connected: {
        m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
        // notify_me will be called which will clean up m_io_handler
        // this logic branch will also call finish_close; check to
        // make sure m_io_handler is still alive, not cleaned up by
        // tcp_io object jumping in and calling notify_me first
        if (m_io_handler) {
          m_io_handler->stop_io();
          return;
        }
        break;
      }
      case timeout: {
        m_timer.cancel();
        break;
      }
      case closing: {
        break;
      }
    }
    finish_close(err);
  }

  void finish_close(const std::error_code& err) {
    m_state = stopped;
    if (err) {
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
    }
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_closed));
  }

  void start_connect(tcp_connector_timeout_func tout_func) {
    m_state = connecting;
    ++m_conn_attempts;
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_connecting));
    auto self = shared_from_this();
    asio::async_connect(m_socket, m_endpoints.cbegin(), m_endpoints.cend(),
          [this, self, tout_func] 
                (const std::error_code& err, endpoints_iter iter) {
        handle_connect(err, iter, tout_func);
      }
    );
  }

  void handle_connect (const std::error_code& err, endpoints_iter /* iter */,
                       tcp_connector_timeout_func tout_func) {
    using namespace std::placeholders;

    if (m_state != connecting) {
      return;
    }
    if (err) {
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
      auto opt_timeout = m_timeout_func(m_conn_attempts);
      if (!opt_timeout) {
        close(std::make_error_code(net_ip_errc::tcp_connector_no_reconnect_attempted));
        return;
      }
      try {
        m_timer.expires_after(*opt_timeout);
      }
      catch (const std::system_error& se) {
        close(se.code());
        return;
      }
      m_state = timeout;
      m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                    std::make_error_code(net_ip_errc::tcp_connector_timeout));
      auto self = shared_from_this();
      m_timer.async_wait( [this, self, tout_func] 
                          (const std::error_code& err) {
          if (err || m_state != timeout) {
            close(err);
            return;
          }
          start_connect(tout_func);
        }
      );
      return;
    }
    m_io_handler = std::make_shared<tcp_io>(std::move(m_socket), 
      tcp_io::entity_notifier_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
    m_state = connected;
    // this is only called after an async connect so no danger of invoking app code during the
    // start method call
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_connected));
    m_entity_common.call_io_state_chg_cb(m_io_handler, 1, true);
    m_conn_attempts = 0u;
  }

  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    // two ways to get here: tcp_io object closed from error, or tcp_io object closed
    // from stop_io, either by tcp_connector stop, or directly by app
    m_io_handler.reset();
    m_entity_common.call_error_cb(iop, err);
    // notify app of tcp_io object shutting down
    m_entity_common.call_io_state_chg_cb(iop, 0, false);
    if (m_state == connected && m_reconn_on_err) {
      start_connect(m_timeout_func);
      return;
    }
    finish_close(std::make_error_code(net_ip_errc::tcp_connector_no_reconnect_attempted));
  }

};

using tcp_connector_shared_ptr = std::shared_ptr<tcp_connector>;
using tcp_connector_weak_ptr = std::weak_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

