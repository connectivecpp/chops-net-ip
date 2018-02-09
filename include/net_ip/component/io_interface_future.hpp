/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that provide a @c std::future that will pull an @c io_interface
 *  object ready for use after a @c net_entity @c start, or two @c io_interface
 *  objects, the first for when @c io_interface is ready and the next for when it is
 *  stopped.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_INTERFACE_FUTURE_HPP_INCLUDED
#define IO_INTERFACE_FUTURE_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move, std::pair
#include <system_error>

#include <future>

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {

namespace detail {

// this function doesn't care about the stop state change
template <typename IO, typename ET>
std::future<IO> make_io_interface_future_impl(ET entity) {
  auto prom = std::promise<IO> { };
  auto fut = prom.get_future();
  entity.start([p = std::move(prom)] (IO io, std::size_t /* sz */) mutable {
      p.set_value(io);
    }
  );
  return fut;
}

template <typename IO, typename ET>
auto make_io_interface_future_pair_impl(ET entity) {
  auto ready_prom = std::promise<IO> { };
  auto ready_fut = ready_prom.get_future();
  auto stop_prom = std::promise<IO> { };
  auto stop_fut = stop_prom.get_future();

  auto start_lam = [ready_p = std::move(ready_prom)] (IO io, std::size_t /* sz */) mutable {
    ready_p.set_value(io);
  };

  auto stop_lam = [stop_p = std::move(stop_prom)] 
                  (IO io, std::error_code /* err */, std::size_t /* sz */) mutable {
    stop_p.set_value(io);
  };

  entity.start(std::move(start_lam), std::move(stop_lam));
  return std::make_pair<std::future<IO>, std::future<IO> >(std::move(ready_fut), std::move(stop_fut));
}

} // end detail namespace

/**
 *  @brief Return a @c std::future for a @c tcp_io_interface after calling @c start on 
 *  the passed in @c tcp_connector_net_entity.
 *
 *  The @c std::future returned from this function can be used in an application to 
 *  block until a TCP connection has completed, the "IO ready" condition. At that 
 *  time a @c tcp_io_interface will be returned as the value from the @c std::future 
 *  and the application can call @c start_io or @c send or other methods on the 
 *  @c tcp_io_interface.

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
  return detail::make_io_interface_future_impl<tcp_io_interface, tcp_connector_net_entity>(conn);
}

/**
 *  @brief Return a @c std::pair of @c std::future objects for a @c tcp_io_interface after 
 *  calling @c start on the passed in @c tcp_connector_net_entity.
 *
 *  The two @c std::future objects returned from this function correspond to the 
 *  "IO ready" condition after start and then "connection stopped" condition.

 *  @param conn A @c tcp_connector_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c std::pair of @c std::future objects, first corresponds to "IO ready", the
 *  second to "stop", both provide a @c tcp_io_interface object.
 */
auto make_tcp_io_interface_future_pair(tcp_connector_net_entity conn) {
  return detail::make_io_interface_future_pair_impl<tcp_io_interface, tcp_connector_net_entity>(conn);
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
  return detail::make_io_interface_future_impl<udp_io_interface, udp_net_entity>(udp_entity);
}

/**
 *  @brief Return a @c std::pair of @c std::future objects for a @c udp_io_interface after 
 *  calling @c start on the passed in @c udp_net_entity.
 *
 *  See comments for @c make_tcp_io_interface_future_pair.
 *
 *  @param udp_entity A @c udp_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c std::pair of @c std::future objects, first corresponds to "IO ready", the
 *  second to "stop", both provide a @c udp_io_interface object.
 *
 */
auto make_udp_io_interface_future_pair(udp_net_entity udp_entity) {
  return std::move(detail::make_io_interface_future_pair_impl<udp_io_interface, udp_net_entity>(udp_entity));
}

} // end net namespace
} // end chops namespace

#endif

