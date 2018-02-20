/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions and classes for error callback handling.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef ERROR_DELIVERY_HPP_INCLUDED
#define ERROR_DELIVERY_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move

#include <system_error>
#include <ostream>

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/io_interface.hpp"

#include "queue/wait_queue.hpp"


namespace chops {
namespace net {

/**
 *  @brief A "do nothing" error function template that can be used in the 
 *  @c basic_net_entity @c start method.
 *
 *  @relates basic_net_entity
 */
template <typename IOT>
void empty_error_func(basic_io_interface<IOT>, std::error_code) { } 

/**
 *  @brief A "do nothing" error function for TCP @c basic_io_interface objects.
 *
 *  @relates basic_net_entity
 */
inline void tcp_empty_error_func(tcp_io_interface, std::error_code) { }

/**
 *  @brief A "do nothing" error function for UDP @c basic_io_interface objects.
 *
 *  @relates basic_net_entity
 */
inline void udp_empty_error_func(udp_io_interface, std::error_code) { }

/**
 *  @brief Data provided through an error function callbacks.
 */
template <typename IOT>
struct error_data {
  basic_io_interface<IOT> io_intf;
  std::error_code         err;
};

/**
 *  @brief @c wait_queue declaration that provides error data.
 */
template <typename IOT>
using err_wait_q = chops::wait_queue<error_data<IOT> >;

/**
 *  @brief Create an error function object that uses a @c wait_queue for error data.
 */
template <typename IOT>
auto make_error_func_with_wait_queue(err_wait_q<IOT>& wq) {
  return [&wq] (basic_io_interface<IOT> io, std::error_code e) {
    wq.emplace_push(io, e);
  };
}

/**
 *  @brief A sink function uses a @c wait_queue for error data and places the data into 
 *  an @c std::ostream.
 *
 *  A @c std::cerr or @c std::cout or @c std::stringstream or similar object can be
 *  used with this function. This function exits when the @c wait_queue closes.
 *
 *  @c std::async can be used to invoke this function in a separate thread.
 *
 *  @param wq A reference to a @c err_wait_q object.
 *
 *  @param os A reference to a @c std::ostream, such as @c std::cerr.
 *
 *  @return The total number of entries processed by the function before the queue is 
 *  closed.
 */
template <typename IOT>
std::size_t ostream_error_sink_with_wait_queue (err_wait_q<IOT>& wq, std::ostream& os) {
  std::size_t cnt = 0;
  bool open = true;
  while (open) {
    auto elem = wq.wait_and_pop();
    if (!elem) {
      open = false;
    }
    else {
      os << "io_addr: " << (*elem).io_intf.get_shared_ptr() << " err: " << 
          (*elem).err << ", " << (*elem).err.message() << '\n';
      ++cnt;
    }
  }
  return cnt;
}

} // end net namespace
} // end chops namespace

#endif

