/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Utility class to manage output data queueing.
 *
 *  Currently @c std::mutex is used to allow multiple threads to concurrently add 
 *  to the output queue. However, there are multiple implementations possible, and
 *  earlier code used @c asio @c post calls to serialize adding to the output 
 *  queue. If performance measurements call for improvements, a lock-free
 *  MPSC queue could be added, or something similar.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef OUTPUT_QUEUE_HPP_INCLUDED
#define OUTPUT_QUEUE_HPP_INCLUDED

#include <queue>
#include <cstddef> // std::size_t
#include <mutex>
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
  mutable std::mutex  m_mutex;

  // std::size_t         m_queue_size;
  // std::size_t         m_total_bufs_sent;
  // std::size_t         m_total_bytes_sent;

private:
  using lg = std::lock_guard<std::mutex>;

public:

  output_queue() noexcept : m_output_queue(), m_current_num_bytes(0u), m_mutex() { }

  // io handlers call this method to get next buffer of data, can be empty
  std::optional<E> get_next_element() {
    lg(m_mutex);
    if (m_output_queue.empty()) {
      return std::optional<E> { };
    }
    E elem = m_output_queue.front();
    m_output_queue.pop();
    m_current_num_bytes -= elem.size();
    return std::optional<E> {elem};
  }

  void add_element(const E& element) {
    lg(m_mutex);
    m_output_queue.push(element);
    m_current_num_bytes += element.size(); // note - possible integer overflow
  }

  chops::net::output_queue_stats get_queue_stats() const noexcept {
    lg(m_mutex);
    return chops::net::output_queue_stats { m_queue.size(), m_current_num_bytes };
  }

  void clear() noexcept {
    lg(m_mutex);
    std::queue<queue_element>().swap(m_output_queue);
  }

};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

