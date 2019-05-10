/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that deliver a @c basic_io_output object, either through @c std::future 
 *  objects or through other mechanisms, such as a @c wait_queue.
 *
 *  When all of the IO processing can be performed in the message handler object, there is
 *  not a need to keep a separate @c basic_io_output object for sending data. But when there is a
 *  need for non-reply sends, component functions in this header package up much of the needed 
 *  logic.
 *
 *  All of these functions take a @c net_entity object and a @c start_io function object,
 *  then call @c start on the @c net_entity using the @c start_io function object and then
 *  return a @c basic_io_output object through various mechanisms.
 *
 *  Empty ("do nothing") error functions are available in the @c error_delivery.hpp header. These
 *  can be used for the error function object parameters.
 *
 *  The @c io_state_change.hpp header provides a collection of functions that create @c start_io
 *  function objects, each packaged with the logic and data needed to call @c start_io.
 *
 *  There are two ways the @c basic_io_output object can be delivered - 1) by @c std::future, or 
 *  2) by a @c wait_queue. Futures are appropriate for TCP connectors and UDP entities, since there 
 *  is only a single state change for IO start and a single state change for IO stop. Futures are
 *  not appropriate for a TCP acceptor, since there are multiple IO start and stop state changes
 *  during the lifetime of the acceptor and futures are single use. For a TCP acceptor the state 
 *  change data is delivered through a @c wait_queue. Obviously a TCP connector or UDP entity 
 *  can also use the @c wait_queue delivery mechanism, which may be more appropriate than futures 
 *  for many use cases.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef IO_OUTPUT_DELIVERY_HPP_INCLUDED
#define IO_OUTPUT_DELIVERY_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move, std::pair
#include <system_error>
#include <memory>

#include <future>

#include "net_ip/net_entity.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"
#include "net_ip/io_type_decls.hpp"

#include "queue/wait_queue.hpp"

namespace chops {
namespace net {

/**
 *  @brief Data provided through an IO state change.
 */
template <typename IOT>
struct io_state_chg_data {
  basic_io_output<IOT>    io_out;
  std::size_t             num_handlers;
  bool                    starting;

  io_state_chg_data(basic_io_output<IOT> io, std::size_t num, bool s) :
      io_out(std::move(io)), num_handlers(num), starting(s) { }
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
 *  calls @c start_io and also passes @c basic_io_output data through a 
 *  @c wait_queue.
 *
 *  @param entity A @c net_entity object, @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on a
 *  @c basic_io_interface object.
 *
 *  @param wq A @c wait_queue which is used to pass the IO state change data.
 *
 *  @param err_func Error function object.
 *
 */
template <typename IOT, typename IOS, typename EF>
void start_with_wait_queue (net_entity entity, 
                            IOS&& io_start,
                            io_wait_q<IOT>& wq, 
                            EF&& err_func) {
  entity.start( [ios = std::move(io_start), &wq]
                   (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
      }
      wq.emplace_push(io.make_io_output(), num, starting);
    },
    std::forward<EF>(err_func)
  );
}

/**
 *  @brief An alias for a @c std::future containing a @c basic_io_output.
 */
template <typename IOT>
using io_output_future = std::future<basic_io_output<IOT> >;

/**
 *  @brief @c io_output_future for TCP IO handlers.
 */
using tcp_io_output_future = io_output_future<tcp_io>;
/**
 *  @brief @c io_output_future for UDP IO handlers.
 */
using udp_io_output_future = io_output_future<udp_io>;


/**
 *  @brief A @c struct containing two @c std::future objects that deliver @c basic_io_output objects
 *  corresponding to the creation and destruction (start, stop) of an IO handler (TCP connection, 
 *  UDP socket).
 *
 *  @note A @c std::pair could be used, but this provides a name for each element.
 */
template <typename IOT>
struct io_output_future_pair {
  io_output_future<IOT>   start_fut;
  io_output_future<IOT>   stop_fut;
};

/**
 *  @brief @c io_output_future_pair for TCP IO handlers.
 */
using tcp_io_output_future_pair = io_output_future_pair<tcp_io>;
/**
 *  @brief @c io_output_future_pair for UDP IO handlers.
 */
using udp_io_output_future_pair = io_output_future_pair<udp_io>;


/**
 *  @brief Return a @c std::future object containing a @c basic_io_output,
 *  which will become available after @c start is called on the passed in 
 *  @c net_entity.
 *
 *  This function returns a single @c std::future object corresponding to when
 *  a TCP connection or UDP socket is created and ready. The @c std::future will 
 *  return a @c basic_io_output object which can be used for sending data.
 *
 *  @param ent A @c net_entity object; @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on a
 *  @c basic_io_interface object.
 *
 *  @param err_func Error function object.
 *
 *  @return An @c io_output_future, either a @c tcp_io_output_future or a 
 *  @c udp_io_output_future.
 *
 *  @note For TCP acceptors this will work for only the first connection that is
 *  created. Once a @c std::promise is delivered it cannot be delivered again.
 */
template <typename IOT, typename IOS, typename EF>
io_output_future<IOT> make_io_output_future(net_entity ent,
                                            IOS&& io_start,
                                            EF&& err_func) {
  // io state change function object must be copyable since it will
  // be stored in a std::function, therefore wrap promise in shared ptr
  auto start_prom_ptr = std::make_shared<std::promise<basic_io_output<IOT> > >();
  auto start_fut = start_prom_ptr->get_future();

  ent.start( [ios = std::move(io_start), start_prom_ptr] 
                    (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
        start_prom_ptr->set_value(io.make_io_output());
      }
    }, std::forward<EF>(err_func)
  );
  return start_fut;
}

/**
 *  @brief Return a pair of @c std::future objects each containing a @c basic_io_output,
 *  which will become available after @c start is called on the passed in 
 *  @c net_entity.
 *
 *  This function returns two @c std::future objects. The first allows the application to
 *  block until a TCP connection or UDP socket is created and ready. At that point the 
 *  @c std::future will return a @c basic_io_output object, and sends can be invoked
 *  as needed.
 *
 *  The second @c std::future will pop when the corresponding connection or socket is 
 *  closed.
 *
 *  @param ent A @c net_entity object; @c start is immediately called.
 *
 *  @param io_start A function object which will invoke @c start_io on a 
 *  @c basic_io_interface object.
 *
 *  @param err_func Error function object.
 *
 *  @return An @c io_output_future_pair, either a @c tcp_io_output_future_pair or
 *  @c udp_io_output_future_pair.
 *
 *  @note For TCP acceptors this will work for only the first connection that is
 *  created. Once a @c std::promise is delivered it cannot be delivered again.
 */

template <typename IOT, typename IOS, typename EF>
io_output_future_pair<IOT> make_io_output_future_pair(net_entity ent,
                                                      IOS&& io_start,
                                                      EF&& err_func) {

  auto start_prom_ptr = std::make_shared<std::promise<basic_io_output<IOT> > >();
  auto start_fut = start_prom_ptr->get_future();
  auto stop_prom_ptr = std::make_shared<std::promise<basic_io_output<IOT> > >();
  auto stop_fut = stop_prom_ptr->get_future();

  ent.start( [ios = std::move(io_start), start_prom_ptr, stop_prom_ptr] 
                (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
      if (starting) {
        ios(io, num, starting);
        start_prom_ptr->set_value(io.make_io_output());
      }
      else {
        stop_prom_ptr->set_value(io.make_io_output());
      }
    }, std::forward<EF>(err_func)
  );

  return io_output_future_pair<IOT> { std::move(start_fut), std::move(stop_fut) };
}

} // end net namespace
} // end chops namespace

#endif

