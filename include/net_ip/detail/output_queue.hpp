/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Utility class to manage output data queueing.
 *
 *  The @c std::atomic counters allow the IO handler to update
 *  while the application queries the stats.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef OUTPUT_QUEUE_HPP_INCLUDED
#define OUTPUT_QUEUE_HPP_INCLUDED

#include <experimental/internet> // std::experimental::net::ip::basic_endpoint

#include <queue>
#include <atomic>
#include <cstddef> // std::size_t
#include <utility> // std::pair
#include <optional>

#include "net_ip/queue_stats.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename Protocol>
class output_queue {
private:

  using opt_endpoint = std::optional<std::experimental::net::ip::basic_endpoint<Protocol> >;
  using queue_element = std::pair<chops::const_shared_buffer, opt_endpoint>;

private:

  std::queue<queue_element> m_output_queue;
  std::atomic_size_t        m_queue_size;
  std::atomic_size_t        m_current_num_bytes;
  // std::size_t               m_total_bufs_sent;
  // std::size_t               m_total_bytes_sent;

public:
  using opt_queue_element = std::optional<queue_element>;

public:

  output_queue() noexcept : m_output_queue(), m_queue_size(0), m_current_num_bytes(0) { }

  // io handlers call this method to get next buffer of data, can be empty
  opt_queue_element get_next_element() {
    if (m_output_queue.empty()) {
      return opt_queue_element { };
    }
    queue_element e = m_output_queue.front();
    m_output_queue.pop();
    --m_queue_size;
    m_current_num_bytes -= e.first.size();
    return opt_queue_element {e};
  }

  void add_element(const chops::const_shared_buffer& buf) {
    add_element(buf, opt_endpoint());
  }

  void add_element(const chops::const_shared_buffer& buf, 
                   const std::experimental::net::ip::basic_endpoint<Protocol>& endp) {
    add_element(buf, opt_endpoint(endp));
  }

  chops::net::output_queue_stats get_queue_stats() const noexcept {
    return chops::net::output_queue_stats { m_queue_size, m_current_num_bytes };
    // return chops::net::output_queue_stats {
    //   m_queue_size, m_current_num_bytes, m_total_bufs_sent, m_total_bytes_sent 
    // };
  }

private:

  void add_element(const chops::const_shared_buffer& buf, opt_endpoint&& opt_endp) {
    m_output_queue.push(queue_element(buf, opt_endp));
    ++m_queue_size;
    m_current_num_bytes += buf.size(); // note - possible integer overflow
    // ++m_total_bufs_sent;
    // m_total_bytes_sent += buf.size();
  }

};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

