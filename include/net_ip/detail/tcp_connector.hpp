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
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include "asio/ip/tcp.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/basic_resolver.hpp"
#include "asio/steady_timer.hpp"

#include <system_error>
#include <vector>
#include <memory>
#include <chrono>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/basic_io_output.hpp"

#include "net_ip/endpoints_resolver.hpp"

// TCP connector has the most complicated states of any of the net entity detail
// objects. The states transition from unstarted to resolving addresses to connecting
// to connected, then back to connecting or stopped depending on the transition. 
// There is also a timeout state for when a connection is refused. As typical, the
// shutdown logic is non-trivial.
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
  enum conn_state { unstarted, resolving, connecting, connected, timeout, stopped };

private:
  net_entity_common<tcp_io>     m_entity_common;
  asio::ip::tcp::socket         m_socket;
  tcp_io_shared_ptr             m_io_handler;
  resolver_type                 m_resolver;
  endpoints                     m_endpoints;
  asio::steady_timer            m_timer;
  std::chrono::milliseconds     m_reconn_time;
  std::string                   m_remote_host;
  std::string                   m_remote_port;
  conn_state                    m_state;

public:
  template <typename Iter>
  tcp_connector(asio::io_context& ioc, 
                Iter beg, Iter end, std::chrono::milliseconds reconn_time) :
      m_entity_common(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(beg, end),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(),
      m_remote_port(),
      m_state(unstarted)
    { }

  tcp_connector(asio::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                std::chrono::milliseconds reconn_time) :
      m_entity_common(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(remote_host),
      m_remote_port(remote_port),
      m_state(unstarted)
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
  std::size_t visit_io_output(F&& f) {
    if (m_io_handler && m_io_handler->is_io_started()) {
      f(basic_io_output<tcp_io>(m_io_handler));
      return 1u;
    }
    return 0u;
  }

  template <typename F1, typename F2>
  std::error_code start(F1&& io_state_chg, F2&& err_cb) {
    if (!m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_cb))) {
      // already started
      return std::make_error_code(net_ip_errc::tcp_connector_already_started);
    }
    // empty endpoints container is the indication that a resolve is needed
    if (m_endpoints.empty()) {
      m_state = resolving;
      m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                    std::make_error_code(net_ip_errc::tcp_connector_resolving_addresses));
      auto self = shared_from_this();
      m_resolver.make_endpoints(false, m_remote_host, m_remote_port,
        [this, self] 
             (std::error_code err, resolver_results res) mutable {
          if (err || m_state == stopped) {
            close(err);
            return;
          }
          for (const auto& e : res) {
            m_endpoints.push_back(e.endpoint());
          }
          clear_strings();
          start_connect();
        }
      );
      return { };
    }
    clear_strings();
    start_connect();
    return { };
  }

  std::error_code stop() {
    if (!m_entity_common.is_started()) {
      // already stopped
      return std::make_error_code(net_ip_errc::tcp_connector_already_stopped);
    }
    close(std::make_error_code(net_ip_errc::tcp_connector_stopped));
    return { };
  }


private:

  void clear_strings() noexcept {
    m_remote_host.clear(); // no longer need the string contents
    m_remote_host.shrink_to_fit();
    m_remote_port.clear();
    m_remote_port.shrink_to_fit();
  }

  bool close(const std::error_code& err) {
    // first one in wins, clean up whatever is in progress
    if (!m_entity_common.stop()) {
      return false; // already closed
    }
    auto sav_state = m_state;
    m_state = stopped;
    switch (sav_state) {
      case unstarted: {
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
        // notify_me will be called which will clean up m_io_handler
        m_io_handler->stop_io();
        break;
      }
      case timeout: {
        m_timer.cancel();
        break;
      }
      case stopped: {
        break;
      }
    }
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_closed));
    return true;
  }

  void start_connect() {
    if (m_state == stopped) {
      return;
    }
    m_state = connecting;
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_connecting));
    auto self = shared_from_this();
    asio::async_connect(m_socket, m_endpoints.cbegin(), m_endpoints.cend(),
          [this, self] 
                (const std::error_code& err, endpoints_iter iter) mutable {
        handle_connect(err, iter);
      }
    );
  }

  void handle_connect (const std::error_code& err, endpoints_iter /* iter */) {
    using namespace std::placeholders;

    if (err) {
      if (m_state == stopped) {
        return;
      }
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
      if (m_reconn_time == std::chrono::milliseconds(0)) {
        close(std::make_error_code(net_ip_errc::tcp_connector_no_reconnect_attempted));
        return;
      }
      try {
        m_timer.expires_after(m_reconn_time);
      }
      catch (const std::system_error& se) {
        close(se.code());
        return;
      }
      m_state = timeout;
      m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                    std::make_error_code(net_ip_errc::tcp_connector_timeout));
      auto self = shared_from_this();
      m_timer.async_wait( [this, self] 
                          (const std::error_code& err) mutable {
          if (err) {
            close(err);
            return;
          }
          start_connect();
        }
      );
      return;
    }
    m_io_handler = std::make_shared<tcp_io>(std::move(m_socket), 
      tcp_io::entity_notifier_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
    m_state = connected;
    std::error_code ec;
    m_socket.close(ec);
    m_entity_common.call_error_cb(tcp_io_shared_ptr(),
                                  std::make_error_code(net_ip_errc::tcp_connector_connected));
    if (!m_entity_common.call_io_state_chg_cb(m_io_handler, 1, true)) {
      // in close method, the connected state logic will kick in
      close(std::make_error_code(net_ip_errc::io_state_change_terminated));
    }
  }

  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    // clean up data member, note that iop keeps the tcp_io object alive for a bit longer
    m_io_handler.reset();
    // if here through stop_io from close, start_connect
    // will exit by checking state, close will not run
    // again because of atomic check
    m_entity_common.call_error_cb(iop, err);
    std::error_code ec;
    if (m_entity_common.call_io_state_chg_cb(iop, 0, false)) {
      if (m_reconn_time != std::chrono::milliseconds(0)) {
        start_connect();
        return;
      }
      ec = std::make_error_code(net_ip_errc::tcp_connector_no_reconnect_attempted);
    }
    else {
      ec = std::make_error_code(net_ip_errc::io_state_change_terminated);
    }
    m_state = stopped;
    // if fall through to here, either io state chg has been terminated, or reconnect
    // time is 0 and no reconnects will be attempted
    close(ec);
  }

};

using tcp_connector_shared_ptr = std::shared_ptr<tcp_connector>;
using tcp_connector_weak_ptr = std::weak_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

