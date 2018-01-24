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

private:
  net_entity_base<tcp_io>    m_entity_base;
  socket_type                m_socket;
  resolver_type              m_resolver;
  endpoints                  m_endpoints;
  chops::periodic_timer<>    m_timer;
  std::size_t                m_reconn_time;
  std::string                m_remote_host;
  std::string                m_remote_port;

public:
  template <typename Iter>
  tcp_connector(std::experimental::net::io_context& ioc, 
                Iter beg, Iter end, std::size_t reconn_time_millis) :
      m_entity_base(),
      m_socket(ioc),
      m_resolver(ioc),
      m_endpoints(beg, end),
      m_timer(ioc),
      m_reconn_time(reconn_time_millis),
      m_remote_host(),
      m_remote_port()
    { }

  tcp_connector(std::experimental::net::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                std::size_t reconn_time_millis) :
      m_entity_base(),
      m_socket(ioc),
      m_resolver(ioc),
      m_endpoints(),
      m_timer(ioc),
      m_reconn_time(reconn_time_millis),
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
    if (m_endpoints.empty()) { // need to get endpoints
      auto self = shared_from_this();
      m_resolver.make_endpoints([this, self] (std::error_code err, resolver_results res) {
            if (err) {
              close();
              m_entity_base.call_shutdown_change_cb(err, tcp_io_ptr());
              return;
            }
            m_endpoints = res;
            start_connect();
          }, false, remote_host, remote_port );
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
    m_entity_base.stop_io_all();
    m_entity_base.stop(); // toggle started flag, clear container of tcp io handlers
    std::error_code ec;
    m_socket.close(ec);
  }

  void start_connect() {
    using namespace std::placeholders;

    auto self = shared_from_this();
    std::experimental::net::async_connect(m_socket, m_endpoints.cbegin(), m_endpoints.cend(),
          [this, self] (const std::error_code& err, endpoints_iter iter) {
             if (err) {






    async_accept( [this, self] (const std::error_code& err,
                                std::experimental::net::ip::tcp::socket sock) {
        if (err) {
          m_entity_base.call_shutdown_change_cb(err, tcp_io_ptr());
          stop();
          return;
        }
        tcp_io_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
        m_entity_base.add_handler(iop);
        m_entity_base.call_start_change_cb(iop);
        start_accept();
      }
    );
  }

  void notify_me(std::error_code err, tcp_io_ptr iop) {
    iop->close();
    m_entity_base.remove_handler(iop);
    m_entity_base.call_shutdown_change_cb(err, iop);
  }

};

using tcp_connector_ptr = std::shared_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

