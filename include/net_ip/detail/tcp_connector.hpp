/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP connector class, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>

#include <system_error>
#include <vector>
#include <memory>
#include <chrono>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"
#include "timer/periodic_timer.hpp"

#include "net_ip/endpoints_resolver.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_connector : public std::enable_shared_from_this<tcp_connector> {
public:
  using socket_type = std::experimental::net::ip::tcp::socket;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;

private:
  using resolver_type = chops::net::endpoints_resolver<std::experimental::net::ip::tcp>;
  using resolver_results = 
    std::experimental::net::ip::basic_resolver_results<std::experimental::net::ip::tcp>;
  using endpoints = std::vector<endpoint_type>;
  using endpoints_iter = endpoints::const_iterator;
  using dur_type = typename chops::periodic_timer<>::duration;

private:
  net_entity_base<tcp_io>    m_entity_base;
  socket_type                m_socket;
  resolver_type              m_resolver;
  endpoints                  m_endpoints;
  chops::periodic_timer<>    m_timer;
  std::chrono::milliseconds  m_reconn_time;
  std::string                m_remote_host;
  std::string                m_remote_port;

public:
  template <typename Iter>
  tcp_connector(std::experimental::net::io_context& ioc, 
                Iter beg, Iter end, std::chrono::milliseconds reconn_time) :
      m_entity_base(),
      m_socket(ioc),
      m_resolver(ioc),
      m_endpoints(beg, end),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(),
      m_remote_port()
    { }

  tcp_connector(std::experimental::net::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                std::chrono::milliseconds reconn_time) :
      m_entity_base(),
      m_socket(ioc),
      m_resolver(ioc),
      m_endpoints(),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(remote_host),
      m_remote_port(remote_port)
    { }

public:

  bool is_started() const noexcept { return m_entity_base.is_started(); }

  socket_type& get_socket() noexcept { return m_socket; }

  template <typename R, typename S>
  void start(R&& start_chg, S&& shutdown_chg) {
    if (!m_entity_base.start(std::forward<R>(start_chg), std::forward<S>(shutdown_chg))) {
      // already started
      return;
    }
    // empty endpoints container is the flag that a resolve is needed
    if (m_endpoints.empty()) {
      auto self = shared_from_this();
      m_resolver.make_endpoints(false, m_remote_host, m_remote_port,
        [this, self] (std::error_code err, resolver_results res) {
          handle_resolve(err, res);
        }
      );
      return;
    }
    start_connect();
  }

  void stop() {
    close();
    m_entity_base.call_shutdown_change_cb(std::make_error_code(net_ip_errc::tcp_connector_stopped), tcp_io_ptr());
  }


private:

  void close() {
    if (!m_entity_base.stop()) {
      return; // stop already called
    }
    m_timer.cancel();
    m_resolver.cancel();
    m_entity_base.stop_io_all();
    m_entity_base.clear_handlers(); // redundant?
    // socket should already be closed or moved from
  }

  void handle_resolve(std::error_code err, resolver_results res) {
    if (err) {
      m_entity_base.call_shutdown_change_cb(err, tcp_io_ptr());
      stop();
      return;
    }
    if (!is_started()) {
      return;
    }
    for (const auto& e : res) {
      m_endpoints.push_back(e.endpoint());
    }
    start_connect();
  }

  void start_connect() {
    auto self = shared_from_this();
    std::experimental::net::async_connect(m_socket, m_endpoints.cbegin(), m_endpoints.cend(),
          [this, self] (const std::error_code& err, endpoints_iter iter) {
        handle_connect(err, iter);
      }
    );
  }

  void handle_connect (const std::error_code& err, endpoints_iter iter) {
    using namespace std::placeholders;

    if (err) {
      m_entity_base.call_shutdown_change_cb(err, tcp_io_ptr());
      auto self = shared_from_this();
      m_timer.start_duration_timer(m_reconn_time,
        [this, self] (std::error_code err, dur_type dur) {
          start_connect();
          return true;
        }
      );
      return;
    }
    tcp_io_ptr iop = std::make_shared<tcp_io>(std::move(m_socket), 
      tcp_io::entity_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
    m_entity_base.add_handler(iop);
    m_entity_base.call_start_change_cb(iop);
  }

  void notify_me(std::error_code err, tcp_io_ptr iop) {
    iop->close();
    m_entity_base.remove_handler(iop);
    m_entity_base.call_shutdown_change_cb(err, iop);
    close();
  }

};

using tcp_connector_ptr = std::shared_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

