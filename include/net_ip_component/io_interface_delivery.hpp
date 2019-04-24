/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that deliver an @c io_interface object, either through @c std::future 
 *  objects or through other mechanisms, such as a @c wait_queue.
 *
 *  When all of the IO processing can be performed in the message handler object, there is
 *  not a need to keep a separate @c io_interface object for sending data. But when there is a
 *  need for non-reply sends, component functions in this header package up much of the needed 
 *  logic.
 *
 *  All of these functions take a @c basic_net_entity object and a @c start_io function object,
 *  then call @c start on the @c basic_net_entity using the @c start_io function object and then
 *  return an @c io_interface object through various mechanisms.
 *
 *  Empty ("do nothing") error functions are available in the @c error_delivery.hpp header. These
 *  can be used for the error function object parameters.
 *
 *  The @c io_state_change.hpp header provides a collection of functions that create @c start_io
 *  function objects, each packaged with the logic and data needed to call @c start_io.
 *
 *  There are two ways the @c io_interface can be delivered - 1) by @c std::future, or 2) by a
 *  @c wait_queue. Futures are appropriate for TCP connectors and UDP entities, since there is
 *  only a single state change for IO start and a single state change for IO stop. Futures are
 *  not appropriate for a TCP acceptor, since there are multiple IO start and stop state changes
 *  during the lifetime of the acceptor and futures are single use. For a TCP acceptor the state 
 *  change data is delivered through a @c wait_queue. Obviously a TCP connector or UDP entity 
 *  can also use the @c wait_queue delivery mechanism, which may be more appropriate than futures 
 *  for many use cases.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
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

#include "queue/wait_queue.hpp"

namespace chops {
namespace net {

/**
 *  @brief Data provided through an IO state change.
 */
template <typename IOT>
struct io_state_chg_data {
  basic_io_interface<IOT> io_intf;
  std::size_t             num_handlers;
  bool                    starting;

  io_state_chg_data(basic_io_interface<IOT> io, std::size_t num, bool s) :
      io_intf(std::move(io)), num_handlers(num), starting(s) { }
};

/**
 *  @brief @c wait_queue declaration that provides IO state change data.
 */
template <typename IOT>
using io_wait_q = chops::wait_queue<io_state_chg_data<IOT> >;

/**
 *  @brief @c io_wait_q for @c tcp_io_interface objects.
 */
using tcp_io_wait_q = io_wait_q<chops::net::tcp_io>;
/**
 *  @brief @c io_wait_q for @c udp_io_interface objects.
 */
using udp_io_wait_q = io_wait_q<chops::net::udp_io>;

/**
 *  @brief Start the entity with an IO state change function object that
 *  calls @c start_io and also passes @c io_interface data through a 
 *  @c wait_queue.
 *
 *  @param entity A @c basic_net_entity object, @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on an 
 *  @c io_interface object.
 *
 *  @param wq A @c wait_queue which is used to pass the IO state change data.
 *
 *  @param err_func Error function object.
 *
 */
template <typename IOT, typename ET, typename IOS, typename EF>
void start_with_wait_queue (basic_net_entity<ET> entity, 
                            IOS&& io_start,
                            io_wait_q<IOT>& wq, 
                            EF&& err_func) {
  entity.start( [ios = std::move(io_start), &wq]
                   (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
      }
      wq.emplace_push(io, num, starting);
    },
    std::forward<EF>(err_func)
  );
}

/**
 *  @brief An alias for a @c std::future containing an @c basic_io_interface.
 */
template <typename IOT>
using io_interface_future = std::future<basic_io_interface<IOT> >;

/**
 *  @brief @c io_interface_future for TCP IO handlers.
 */
using tcp_io_interface_future = io_interface_future<tcp_io>;
/**
 *  @brief @c io_interface_future for UDP IO handlers.
 */
using udp_io_interface_future = io_interface_future<udp_io>;


/**
 *  @brief A @c struct containing two @c std::future objects that deliver an @c io_interface 
 *  corresponding to the creation and destruction (start, stop) of an IO handler (TCP connection, 
 *  UDP socket).
 *
 *  @note A @c std::pair could be used, but this provides a name for each element.
 */
template <typename IOT>
struct io_interface_future_pair {
  io_interface_future<IOT>   start_fut;
  io_interface_future<IOT>   stop_fut;
};

/**
 *  @brief @c io_interface_future_pair for TCP IO handlers.
 */
using tcp_io_interface_future_pair = io_interface_future_pair<tcp_io>;
/**
 *  @brief @c io_interface_future_pair for UDP IO handlers.
 */
using udp_io_interface_future_pair = io_interface_future_pair<udp_io>;


namespace detail {

template <typename IOT>
using io_prom = std::promise<basic_io_interface<IOT> >;

// io state change function object must be copyable since it will
// be stored in a std::function, therefore wrap promise in shared ptr
template <typename IOT, typename ET, typename IOS, typename EF>
auto make_io_interface_future_impl(basic_net_entity<ET> entity,
                                   IOS&& io_start,
                                   EF&& err_func) {
  auto start_prom_ptr = std::make_shared<io_prom<IOT> >();
  auto start_fut = start_prom_ptr->get_future();

  entity.start( [ios = std::move(io_start), start_prom_ptr] 
                    (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
        start_prom_ptr->set_value(io);
      }
    }, std::forward<EF>(err_func)
  );
  return start_fut;
}

template <typename IOT, typename ET, typename IOS, typename EF>
auto make_io_interface_future_pair_impl(basic_net_entity<ET> entity,
                                        IOS&& io_start,
                                        EF&& err_func) {

  auto start_prom_ptr = std::make_shared<io_prom<IOT> >();
  auto start_fut = start_prom_ptr->get_future();
  auto stop_prom_ptr = std::make_shared<io_prom<IOT> >();
  auto stop_fut = stop_prom_ptr->get_future();

  entity.start( [ios = std::move(io_start), start_prom_ptr, stop_prom_ptr] 
                    (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
        start_prom_ptr->set_value(io);
      }
      else {
        stop_prom_ptr->set_value(io);
      }
    }, std::forward<EF>(err_func)
  );

  return io_interface_future_pair<IOT> { std::move(start_fut), std::move(stop_fut) };
}

} // end detail namespace


/**
 *  @brief Return a @c std::future object containing a @c tcp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c tcp_connector_net_entity.
 *
 *  This function returns a single @c std::future object corresponding to when
 *  a TCP connection is created and ready. The @c std::future will return a 
 *  @c tcp_io_interface object which can be used for sends and other operations.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on an 
 *  @c io_interface object.
 *
 *  @param err_func Error function object.
 *
 *  @return A @c tcp_io_interface_future.
 *
 *  @note There is not an equivalent function for a @c tcp_acceptor_net_entity, 
 *  since multiple connections are typically created and a @c std::promise and
 *  corresponding @c std::future can only be fulfilled once.
 */
template <typename IOS, typename EF>
tcp_io_interface_future make_tcp_io_interface_future(tcp_connector_net_entity conn,
                                                     IOS&& io_start,
                                                     EF&& err_func) {
  return detail::make_io_interface_future_impl<tcp_io, 
                        detail::tcp_connector, IOS, EF>(conn,
                                                        std::forward<IOS>(io_start),
                                                        std::forward<EF>(err_func));
}

/**
 *  @brief Return a pair of @c std::future objects containing @c tcp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c tcp_connector_net_entity.
 *
 *  This function returns two @c std::future objects. The first allows the application to
 *  block until a TCP connection is created and ready. At that point the @c std::future will 
 *  return a @c tcp_io_interface object, and sends and other operations can be invoked
 *  as needed.
 *
 *  The second @c std::future will pop when the corresponding TCP connection is closed.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on an 
 *  @c io_interface object.
 *
 *  @param err_func Error function object.
 *
 *  @return A @c tcp_io_interface_future_pair.
 *
 */

template <typename IOS, typename EF>
auto make_tcp_io_interface_future_pair(tcp_connector_net_entity conn,
                                       IOS&& io_start,
                                       EF&& err_func) {
  return detail::make_io_interface_future_pair_impl<tcp_io,
                        detail::tcp_connector, IOS, EF>(conn,
                                                        std::forward<IOS>(io_start),
                                                        std::forward<EF>(err_func));


}

/**
 *  @brief Return a @c std::future object containing a @c udp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c udp_net_entity.
 *
 *  The @c std::future returned from this function can be used in an application to 
 *  block until UDP processing is ready (typically a local bind, if needed). At that 
 *  time a @c udp_io_interface will be returned as the value from the @c std::future and 
 *  the application can performs sends and other operations as needed.
 *
 *  @param udp_entity A @c udp_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @param io_start A function object which will invoke @c start_io on an 
 *  @c io_interface object.
 *
 *  @param err_func Error function object.
 *
 *  @return A @c std::future which will provide a @c udp_io_interface when ready.
 *
 */
template <typename IOS, typename EF>
udp_io_interface_future make_udp_io_interface_future(udp_net_entity udp_entity,
                                                     IOS&& io_start,
                                                     EF&& err_func) {
  return detail::make_io_interface_future_impl<udp_io, 
                        detail::udp_entity_io, IOS, EF>(udp_entity,
                                                        std::forward<IOS>(io_start),
                                                        std::forward<EF>(err_func));
}

/**
 *  @brief Return a pair of @c std::future objects containing @c udp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c udp_net_entity.
 *
 *  See comments for @c make_tcp_io_interface_future_pair.
 *
 *  @return A @c udp_io_interface_future_pair.
 *
 */
template <typename IOS, typename EF>
auto make_udp_io_interface_future_pair(udp_net_entity udp_entity,
                                       IOS&& io_start,
                                       EF&& err_func) {
  return detail::make_io_interface_future_pair_impl<udp_io,
                        detail::udp_entity_io, IOS, EF>(udp_entity,
                                                        std::forward<IOS>(io_start),
                                                        std::forward<EF>(err_func));


}

} // end net namespace
} // end chops namespace

#endif

