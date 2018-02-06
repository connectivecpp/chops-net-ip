/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that provide a @c std::future that will eventually return a 
 *  ready @c io_interface, after a @c net_entity @c start.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_INTERFACE_FUTURE_HPP_INCLUDED
#define IO_INTERFACE_FUTURE_HPP_INCLUDED

#include <cstddef> // std::size_t

#include <future>

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {

/**
 *  @brief Return a @c std::future for a @c tcp_io_interface after calling @c start on 
 *  the passed in @c tcp_connector_net_entity.
 *
 *  The @c std::future returned from this function can be used in an application to 
 *  block until a TCP connection has completed. At that time a @c tcp_io_interface 
 *  will be returned as the value from the @c std::future and the application can
 *  call @c start_io or @c send or other methods on the @c tcp_io_interface.

 *  @param conn A @c tcp_connector_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c std::future which will provide a @c tcp_io_interface when ready.
 *
 *  @note There is not an equivalent function for a @c tcp_acceptor_net_entity, 
 *  since multiple connections can be potentially made and a @c std::promise and
 *  corresponding @c std::future can only be fulfilled once.
 */
std::future<tcp_io_interface> make_tcp_io_interface_future(tcp_connector_net_entity conn) {
  return make_io_interface_impl<tcp_io_interface, tcp_connector_net_entity>(conn);
}


/**
 *  @brief Return a @c std::future for a @c udp_io_interface after calling @c start on 
 *  the passed in @c udp_net_entity.
 *
 *  The @c std::future returned from this function can be used in an application to 
 *  block until UDP processing is ready (typically a local bind, if needed). At that 
 *  time a @c udp_io_interface will be returned as the value from the @c std::future and 
 *  the application can call @c start_io or @c send or other methods on the 
 *  @c udp_io_interface.
 *
 *  @param udp_entity A @c udp_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c std::future which will provide a @c udp_io_interface when ready.
 *
 */
std::future<udp_io_interface> make_udp_io_interface_future(udp_net_entity udp_entity) {
  return make_io_interface_impl<udp_io_interface, udp_net_entity>(udp_entity);
}

template <typename IO, typename ET>
std::future<IO> make_io_interface_impl(ET entity) {
  auto prom = std::promise<IO> { };
  auto fut = prom.get_future();
  entity.start([p = std::move(prom)] (IO io, std::size_t /* sz */) mutable {
      p.set_value(io);
    }
  ); // don't care about stop function
  return fut;
}

} // end net namespace
} // end chops namespace

#endif

