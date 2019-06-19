/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP and UDP io handlers.
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

#ifndef IO_COMMON_HPP_INCLUDED
#define IO_COMMON_HPP_INCLUDED

#include <atomic>
#include <optional>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename E>
class io_common {
private:
  std::atomic_bool     m_io_started; // may be called from multiple threads concurrently
  bool                 m_write_in_progress; // internal only, doesn't need to be atomic
  output_queue<E>      m_outq;

public:
  enum write_status { io_stopped, queued, not_queued };

public:

  io_common() noexcept :
    m_io_started(false), m_write_in_progress(false), m_outq() { }

  // the following four methods can be called concurrently
  auto get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  bool is_io_started() const noexcept { return m_io_started; }

  bool set_io_started() noexcept {
    bool expected = false;
    return m_io_started.compare_exchange_strong(expected, true);
  }

  bool set_io_stopped() noexcept {
    bool expected = true;
    return m_io_started.compare_exchange_strong(expected, false); 
  }

  // rest of these method called only from within run thread
  bool is_write_in_progress() const noexcept { return m_write_in_progress; }

  write_status start_write_setup(const E&);

  std::optional<E> get_next_element();

  void clear() noexcept {
    m_outq.clear();
  }

};

template <typename E>
auto io_common<E>::start_write_setup(const E& elem) {
  if (!m_io_started) {
    return io_stopped; // shutdown happening or not io_started, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(buf);
    return queued;
  }
  m_write_in_progress = true;
  return not_queued;
}

template <typename E>
std::optional<E> io_common<IOT>::get_next_element() {
  if (!m_io_started) { // shutting down
    clear();
    return std::optional<E> { };
  }
  auto elem = m_outq.get_next_element();
  m_write_in_progress = elem.has_value();
  return elem;
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

