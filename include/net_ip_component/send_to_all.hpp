/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief A class template that manages a collection of 
 *  @c basic_io_interface objects and provides "send to all" functionality.
 *
 *  @author Cliff Green, Thurman Gillespy
 *  @date 2019-10-28
 *
 *  Copyright (c) 2019 by Cliff Green, Thurman Gillespy
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef SEND_TO_ALL_HPP_INCLUDED
#define SEND_TO_ALL_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move

#include <mutex>
#include <vector>

// #include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"
#include "net_ip/queue_stats.hpp"

#include "utility/erase_where.hpp"
#include "marshall/shared_buffer.hpp"

namespace chops {
namespace net {

/**
 *  @brief Manage a collection of @c basic_io_output objects and provide a way
 *  to send data to all. or to all except a specific object.
 *
 *  In some use cases a buffer of data needs to be sent to multiple TCP connections or 
 *  UDP destinations. This class manages a collection of @c basic_io_output objects
 *  and simplifies sending to all of them, or to all except a specific object.
 *
 *  In particular, if the buffer of data to be sent is not yet in a reference counted buffer 
 *  and the @c void pointer interface is used, only one buffer copy is made, and all TCP 
 *  connections or UDP sockets will shared the same reference counted buffer, saving buffer 
 *  copies across all of the connections or UDP sockets.
 *
 *  A function object operator overload is provided so that a @c std::ref to a @c send_to_all
 *  object can be used in composing function objects for @c io_state_change calls.
 *
 *  This class is thread-safe for concurrent access.
 *
 */
template <typename IOT>
class send_to_all {
private:
  using lock_guard  = std::lock_guard<std::mutex>;
  // using io_intf    = basic_io_interface<IOT>;
  using io_out      = chops::net::basic_io_output<IOT>;
  using io_outs     = std::vector<io_out>;

private:
  mutable std::mutex    m_mutex;
  io_intfs              m_io_intfs;

public:
/**
 *  @brief Add a @c basic_io_interface object to the collection.
 */
  void add_io_interface(io_out io) {
    lock_guard gd { m_mutex };
    m_io_intfs.push_back(io);
  }

/**
 *  @brief Remove a @c basic_io_interface object to the collection.
 */
  void remove_io_interface(io_out io) {
    lock_guard gd { m_mutex };
    chops::erase_where(m_io_intfs, io);
  }

/**
 *  @brief Interface for @c io_state_change parameter of @c start
 *  method.
 */
  void operator() (io_out io, std::size_t, bool starting) {
    if (starting) {
      add_io_interface(io);
    }
    else {
      remove_io_interface(io);
    }
  }

/**
 *  @brief Send a reference counted buffer to all @c basic_io_interface
 *  objects.
 */
  void send(chops::const_shared_buffer buf) const {
    lock_guard gd { m_mutex };
    for (const auto& io : m_io_intfs) {
      io.send(buf);
    }
  }

/**
 *  @brief Send a reference counted buffer to all @c basic_io_interface
 *  objects except @c cur_io.
 */
  void send(chops::const_shared_buffer buf, io_out cur_io) const { // TG
    lock_guard gd { m_mutex };
    for (const auto& io : m_io_intfs) {
      if ( !(cur_io == io) ) {
        io.send(buf);
      }
    }
  }

/**
 *  @brief Copy the bytes, create a reference counted buffer, then send it to
 *  all @c basic_io_interface objects.
 */
  void send(const void* buf, std::size_t sz) const {
    send(chops::const_shared_buffer(buf, sz));
  }

/**
 *  @brief Copy the bytes, create a reference counted buffer, then send it to
 *  all @c basic_io_interface objects except @c cur_io.
 */
  void send(const void* buf, std::size_t sz, io_out cur_io) const { // TG
    send(chops::const_shared_buffer(buf, sz), cur_io);
  }

/**
 *  @brief Move the buffer from a writable reference counted buffer to a 
 *  immutable reference counted buffer, then send to all.
 */
  void send(chops::mutable_shared_buffer&& buf) const { 
    send(chops::const_shared_buffer(std::move(buf)));
  }

/**
 *  @brief Move the buffer from a writable reference counted buffer to a 
 *  immutable reference counted buffer, then send to all except @c cur_io.
 */
  void send(chops::mutable_shared_buffer&& buf, io_out cur_io) const { 
    send(chops::const_shared_buffer(std::move(buf)), cur_io);
  }

/**
 *  @brief Return the number of @c basic_io_interface objects in the collection.
 */
  std::size_t size() const noexcept {
    lock_guard gd { m_mutex };
    return m_io_intfs.size();
  }

/**
 *  @brief Return the sum total of output queue statistics.
 *
 *  @return @c output_queue_stats object containing total counts.
 */
  auto get_total_output_queue_stats() const noexcept {
    chops::net::output_queue_stats tot { };
    lock_guard gd { m_mutex };
    for (const auto& io : m_io_intfs) {
      auto qs = io.get_output_queue_stats();
      tot.output_queue_size += qs.output_queue_size;
      tot.bytes_in_output_queue += qs.bytes_in_output_queue;
    }
    return tot;
  }
};

} // end net namespace
} // end chops namespace

#endif

