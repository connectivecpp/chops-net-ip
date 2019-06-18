/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions and classes for error callback handling and basic logging.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef ERROR_DELIVERY_HPP_INCLUDED
#define ERROR_DELIVERY_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move

#include <system_error>
#include <ostream>
#include <string>
#include <chrono>

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

#include "queue/wait_queue.hpp"


namespace chops {
namespace net {

/**
 *  @brief Data provided through an error function callback.
 *
 *  This @c struct of data can be passed through a queue or other mechanism for logging
 *  or other error analysis purposes.
 *
 *  A @c basic_io_interface or @c basic_io_output is not part of the data since the referenced 
 *  handler is likely to soon go away. Instead, a @c void pointer of the underlying
 *  handler is used. Another reason for not storing a @c basic_io_interface is that
 *  the IO handler type parameterization is no longer needed, so this can be used for
 *  both TCP and UDP error data. The pointer address is used for logging purposes only.
 */
struct error_data {
  std::chrono::steady_clock::time_point   time_p;
  const void*                             io_intf_ptr;
  std::error_code                         err;

  error_data(const void* iop, std::error_code e) : 
      time_p(std::chrono::steady_clock::now()), io_intf_ptr(iop), err(std::move(e)) { }
};

/**
 *  @brief @c wait_queue declaration that provides error data.
 */
using err_wait_q = chops::wait_queue<error_data>;

/**
 *  @brief Create an error function object that uses a @c wait_queue for error data.
 */
template <typename IOT>
auto make_error_func_with_wait_queue(err_wait_q& wq) {
  return [&wq] (basic_io_interface<IOT> io, std::error_code e) {
    wq.emplace_push(io.get_ptr(), e);
  };
}

/**
 *  @brief A sink function that uses a @c wait_queue for error data and streams the data 
 *  into an @c std::ostream.
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
inline std::size_t ostream_error_sink_with_wait_queue (err_wait_q& wq, std::ostream& os) {
  std::size_t cnt = 0;
  while (true) {
    auto elem = wq.wait_and_pop();
    if (!elem) {
      break;
    }

    auto t =
      std::chrono::duration_cast<std::chrono::milliseconds>((*elem).time_p.time_since_epoch()).count();

    os << '[' << t << "] io_addr: " << elem->io_intf_ptr << " err: " << 
          elem->err << ", " << (*elem).err.message() << '\n';
    ++cnt;
  }
  os.flush();
  return cnt;
}

} // end net namespace
} // end chops namespace

#endif

