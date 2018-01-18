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
private:
  using resolver_type = chops::net::endpoints_resolver<std::experimental::net::ip::tcp>;
  using endpoints = std::vector<std::experimental::net::ip::tcp::endpoint>;

private:
  net_entity_base<tcp_io>                 m_entity_base;
  std::experiment::net::ip::tcp::socket   m_socket;
  resolver_type                           m_resolver;
  endpoints                               m_endpoints;
  chops::periodic_timer<>                 m_timer;
  std::size_t                             m_reconn_time;
  std::string                             m_remote_host;
  std::string                             m_remote_port;

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

};


} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

