/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that deliver an @c io_interface object, either through @c std::future 
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

/**
 *  @brief A @c std::future containing an @c io_interface.
 */
template <typename IOH>
using io_interface_future = std::future<basic_io_interface<IOH> >;

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
 *  corresponding to the creation and destruction of an IO handler (TCP connection, UDP socket).
 *
 *  @note A @c std::pair could be used, but this provides a name for each element.
 */
template <typename IOH>
struct io_interface_future_pair {
  io_interface_future<IOH>   start_fut;
  io_interface_future<IOH>   stop_fut;
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

template <typename IOH>
using io_prom = std::promise<basic_io_interface<IOH> >;

// the state change function object must be copyable since internally it is stored in
// a std::function for later invocation
template <typename IOH>
struct fut_state_chg_cb {

  std::shared_ptr<io_prom<IOH> >  m_start_prom;
  std::shared_ptr<io_prom<IOH> >  m_stop_prom;

  fut_state_chg_cb(io_prom<IOH> start_prom, io_prom<IOH> stop_prom) : 
        m_start_prom(std::make_shared<io_prom<IOH> >(std::move(start_prom))), 
        m_stop_prom(std::make_shared<io_prom<IOH> >(std::move(stop_prom)))     { }

  fut_state_chg_cb(io_prom<IOH> start_prom) : 
        m_start_prom(std::make_shared<io_prom<IOH> >(std::move(start_prom))), 
        m_stop_prom()     { }


  void operator()(basic_io_interface<IOH> io, std::size_t /* sz */, bool starting) {
    if (starting) {
      m_start_prom->set_value(io);
    }
    else {
      if (m_stop_prom) {
        m_stop_prom->set_value(io);
      }
    }
  }
};

// this function doesn't care about the stop state change or the error callback
template <typename IOH, typename ET>
io_interface_future<IOH> make_io_interface_future_impl(basic_net_entity<ET> entity) {
  auto start_prom = io_prom<IOH> { };
  auto start_fut = start_prom.get_future();

  entity.start( fut_state_chg_cb<IOH>(std::move(start_prom)) );

  return start_fut;
}

// this function doesn't care about the error callback
template <typename IOH, typename ET>
auto make_io_interface_future_pair_impl(basic_net_entity<ET> entity) {

  auto start_prom = io_prom<IOH> { };
  auto stop_prom = io_prom<IOH> { };

  io_interface_future_pair<IOH> fp { start_prom.get_future(), stop_prom.get_future() };

  entity.start( fut_state_chg_cb<IOH>(std::move(start_prom), std::move(stop_prom)) );

  return fp;
}

} // end detail namespace


/**
 *  @brief Return a @c std::future object containing a @c tcp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c tcp_connector_net_entity.
 *
 *  This function returns a single @c std::future object corresponding to when
 *  a TCP is created and ready. The @c std::future will return a @c tcp_io_interface 
 *  object, and @c start_io and other methods can be called as needed.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start is immediately called.
 *
 *  @return A @c tcp_io_interface_future.
 *
 *  @note There is not an equivalent function for a @c tcp_acceptor_net_entity, 
 *  since multiple connections are typically created and a @c std::promise and
 *  corresponding @c std::future can only be fulfilled once.
 */
tcp_io_interface_future make_tcp_io_interface_future(tcp_connector_net_entity conn) {
  return detail::make_io_interface_future_impl<tcp_io>(conn);
}
/**
 *  @brief Return a pair of @c std::future objects containing @c tcp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c tcp_connector_net_entity.
 *
 *  This function returns two @c std::future objects. The first allows the application to
 *  block until a TCP connection is created and ready. At that point the @c std::future will 
 *  return a @c tcp_io_interface object, and @c start_io and other methods can be called
 *  as needed.
 *
 *  The second @c std::future will pop when the corresponding TCP connection is closed.
 *
 *  @param conn A @c tcp_connector_net_entity object; @c start is immediately called.
 *
 *  @return A @c tcp_io_interface_future_pair.
 *
 */

auto make_tcp_io_interface_future_pair(tcp_connector_net_entity conn) {
  return detail::make_io_interface_future_pair_impl<tcp_io>(conn);
}

/**
 *  @brief Return a @c std::future object containing a @c udp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c udp_net_entity.
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
udp_io_interface_future make_udp_io_interface_future(udp_net_entity udp_entity) {
  return detail::make_io_interface_future_impl<udp_io>(udp_entity);
}

/**
 *  @brief Return a pair of @c std::future objects containing @c udp_io_interface,
 *  which will become available after @c start is called on the passed in 
 *  @c udp_net_entity.
 *
 *  See comments for @c make_tcp_io_interface_future_pair.
 *
 *  @param udp_entity A @c udp_net_entity object; @c start will immediately be
 *  called on it.
 *
 *  @return A @c udp_io_interface_future_pair.
 *
 */
auto make_udp_io_interface_future_pair(udp_net_entity udp_entity) {
  return detail::make_io_interface_future_pair_impl<udp_io>(udp_entity);
}

} // end net namespace
} // end chops namespace

#endif

