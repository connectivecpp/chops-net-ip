/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Utility class to manage output data queueing.
 *
 *  Concurrency protection is needed at a higher level to enforce data structure and flag
 *  consistency for data sending, as well as to ensure that only one write is in process at 
 *  a time. There are multiple ways to accomplish this goal, whether with locks (mutex or 
 *  spin-lock or semaphore, etc), or by posting all write operations through the @c asio 
 *  executor.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef OUTPUT_QUEUE_HPP_INCLUDED
#define OUTPUT_QUEUE_HPP_INCLUDED

#include <queue>
#include <cstddef> // std::size_t
#include <optional>

#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {
namespace detail {

// template parameter E is instantiated as either a shared_buffer or a shared_buffer and 
// endpoint, depending on IO handler; a size() method is expected for E
template <typename E>
class output_queue {
private:

  std::queue<E>       m_output_queue;
  std::size_t         m_current_num_bytes;

  // std::size_t         m_queue_size;
  // std::size_t         m_total_bufs_sent;
  // std::size_t         m_total_bytes_sent;

public:

  output_queue() noexcept : m_output_queue(), m_current_num_bytes(0u) { }

  // io handlers call this method to get next buffer of data, can be empty
  std::optional<E> get_next_element() {
    if (m_output_queue.empty()) {
      return std::optional<E> { };
    }
    E elem = m_output_queue.front();
    m_output_queue.pop();
    m_current_num_bytes -= elem.size();
    return std::optional<E> {elem};
  }

  void add_element(const E& element) {
    m_output_queue.push(element);
    m_current_num_bytes += element.size(); // note - possible integer overflow
  }

  chops::net::output_queue_stats get_queue_stats() const noexcept {
    return chops::net::output_queue_stats { m_output_queue.size(), m_current_num_bytes };
  }

  void clear() noexcept {
    std::queue<E>().swap(m_output_queue);
    m_current_num_bytes = 0u;
  }

};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

