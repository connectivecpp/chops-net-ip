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

#include <cassert>

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
  net_entity_common<tcp_io>     m_entity_common;
  asio_ip::tcp_socket           m_socket;
  tcp_io_shared_ptr             m_io_handler;
  resolver_type                 m_resolver;
  endpoints                     m_endpoints;
  asio::steady_timer            m_timer;
  std::chrono::milliseconds     m_reconn_time;
  std::string                   m_remote_host;
  std::string                   m_remote_port;

  // TODO: currently this flag is needed to distinguish whether a connect
  // handler can't connect or whether the operation is cancelled and it's
  // time to shutdown
  bool                                  m_shutting_down;

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
      m_shutting_down(false)
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
      m_shutting_down(false)
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
  void visit_io_output(F&& f) {
    if (m_io_handler) {
      f(basic_io_output(m_io_handler));
    }
  }

  template <typename F1, typename F2>
  bool start(F1&& io_state_chg, F2&& err_cb) {
    if (!m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_cb))) {
      // already started
      return false;
    }
    m_shutting_down = false;
    // empty endpoints container is the flag that a resolve is needed
    if (m_endpoints.empty()) {
      auto self = shared_from_this();
      m_resolver.make_endpoints(false, m_remote_host, m_remote_port,
        [this, self] 
             (std::error_code err, resolver_results res) mutable {
          if (err) {
            m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
            m_entity_common.stop();
            return;
          }
          for (const auto& e : res) {
            m_endpoints.push_back(e.endpoint());
          }
          start_connect();
        }
      );
      return true;
    }
    start_connect();
    return true;
  }

  bool stop() {
    if (!close()) {
      return false;
    }
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), std::make_error_code(net_ip_errc::tcp_connector_stopped));
    return true;
  }


private:

  bool close() {
    if (!m_entity_common.stop()) {
      return false; // stop already called
    }
    m_shutting_down = true;
    if (m_endpoints.empty()) { // may be in middle of resolve
      m_resolver.cancel();
    }
    if (m_io_handler) {
      if (m_io_handler->is_io_started()) {
        m_io_handler->close();
      }
      m_io_handler.reset();
    }
    else {
      // IO handler not created, may be waiting on timer
      // or in middle of an async connect
      m_timer.cancel();
    }
    std::error_code ec;
    m_socket.close(ec);
    return true;
  }

  void start_connect() {
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
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
      if (!is_started() || m_shutting_down ) {
//      if (!is_started() || err.value() == something) {
        return;
      }
      try {
        m_timer.expires_after(m_reconn_time);
      }
      catch (const std::system_error& se) {
        m_entity_common.call_error_cb(tcp_io_shared_ptr(), se.code());
        m_entity_common.stop();
        return;
      }
      auto self = shared_from_this();
      m_timer.async_wait( [this, self] 
                          (const std::error_code& err) mutable {
          if (!err) {
            start_connect();
          }
        }
      );
      return;
    }
    m_io_handler = std::make_shared<tcp_io>(std::move(m_socket), 
      tcp_io::entity_notifier_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
    m_entity_common.call_io_state_chg_cb(m_io_handler, 1, true);
  }

  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    assert (iop == m_io_handler);

    iop->close();
    m_entity_common.call_error_cb(iop, err);
    m_entity_common.call_io_state_chg_cb(iop, 0, false);
    stop();
  }

};

using tcp_connector_shared_ptr = std::shared_ptr<tcp_connector>;
using tcp_connector_weak_ptr = std::weak_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

