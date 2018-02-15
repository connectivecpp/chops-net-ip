/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that provide an @c io_interface object, either through @c std::future 
 *  objects or through other mechanisms, such as a @c wait_queue.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_INTERFACE_DELIVERY_HPP_INCLUDED
#define IO_INTERFACE_DELIVERY_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move, std::pair
#include <system_error>
#include <memory>

#include <future>

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {


template <typename IOH>
struct io_interface_future_pair {
  std::future<basic_io_interface<IOH> > start_fut;
  std::future<basic_io_interface<IOH> > stop_fut;
};

using tcp_io_interface_future_pair = io_interface_future_pair<tcp_io>;
using udp_io_interface_future_pair = io_interface_future_pair<udp_io>;

namespace detail {

template <typename IOH>
using io_fut = std::future<basic_io_interface<IOH> >;

template <typename IOH>
using io_prom = std::promise<basic_io_interface<IOH> >;

// this function doesn't care about the stop state change
template <typename IOH, typename ET>
io_fut<IOH> make_io_interface_future_impl(basic_net_entity<ET> entity) {
  auto prom = io_prom<IOH> { };
  auto fut = prom.get_future();
  entity.start([p = std::move(prom)] (basic_io_interface<IOH> io, std::size_t /* sz */) mutable {
      p.set_value(io);
    }
  );
  return fut;
}

// the state change function object must be copyable since internally it is stored in
// a std::function for later invocation
template <typename IOH>
struct fut_state_chg_cb {

  std::shared_ptr<io_prom<IOH> >  m_start_prom;
  std::shared_ptr<io_prom<IOH> >  m_stop_prom;

  stop_cb(io_prom<IOH> prom) : m_prom(std::make_shared<io_prom<IOH> >(std::move(prom))), 
                               m_satisfied(false) { }

  void operator()(basic_io_interface<IOH> io, std::error_code /* err */, std::size_t /* sz */) {
    if (!m_satisfied) {
      m_satisfied = true;
      m_prom->set_value(io);
    }
  }
};


template <typename IOH, typename ET>
std::pair<io_fut<IOH>, io_fut<IOH> > make_io_interface_future_pair_impl(basic_net_entity<ET> entity) {

  auto ready_prom = io_prom<IOH> { };
  auto ready_fut = ready_prom.get_future();
  auto stop_prom = io_prom<IOH> { };
  auto stop_fut = stop_prom.get_future();

  entity.start( [ready_p = std::move(ready_prom)] (basic_io_interface<IOH> io, std::size_t /* sz */) mutable {
      ready_p.set_value(io);
    }, 
    stop_cb<IOH>(std::move(stop_prom))
  );

  return std::make_pair<io_fut<IOH>, io_fut<IOH> >(std::move(ready_fut), std::move(stop_fut));
}

} // end detail namespace

/**
 *  @brief Return a pair of @c std::future objects instantiated on @c tcp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c tcp_connector_net_entity.
 *
 *  This function returns two @c std::future objects. The first allows the application to
 *  block until a TCP connection is created and ready and a @c tcp_io_interface object
 *  is returned from the @c std::future. At this point @c start_io can be called, and then 
 *  @c send or other methods called as needed. 
 *
 *  The second @c std::future will pop when the corresponding TCP connection is closed.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start is immediately called.
 *
 *  @return A @c std::future which will provide a @c tcp_io_interface when ready.
 *
 *  @note There is not an equivalent function for a @c tcp_acceptor_net_entity, 
 *  since multiple connections can be potentially made and a @c std::promise and
 *  corresponding @c std::future can only be fulfilled once.
 */
detail::io_fut<tcp_io> make_tcp_io_interface_future(tcp_connector_net_entity conn) {
  return detail::make_io_interface_future_impl<tcp_io>(conn);
}

/**
 *  @brief Return a @c std::pair of @c std::future objects for a @c tcp_io_interface after 
 *  calling @c start on the passed in @c tcp_connector_net_entity.
 *
 *  The two @c std::future objects returned from this function correspond to the 
 *  "IO ready" condition after start and then "connection stopped" condition.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c std::pair of @c std::future objects, first corresponds to "IO ready", the
 *  second to "stop", both provide a @c tcp_io_interface object.
 */
auto make_tcp_io_interface_future_pair(tcp_connector_net_entity conn) {
  return detail::make_io_interface_future_pair_impl<tcp_io>(conn);
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
detail::io_fut<udp_io> make_udp_io_interface_future(udp_net_entity udp_entity) {
  return detail::make_io_interface_future_impl<udp_io>(udp_entity);
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
  return detail::make_io_interface_future_pair_impl<udp_io>(udp_entity);
}

} // end net namespace
} // end chops namespace

#endif

