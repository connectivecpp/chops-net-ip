/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Factored common code for TCP and UDP io handlers.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_COMMON_HPP_INCLUDED
#define IO_COMMON_HPP_INCLUDED

#include <atomic>
#include <system_error>

#include <experimental/internet>
#include <experimental/socket>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/queue_stats.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename Protocol>
class io_common {
private:

  std::atomic_bool                            m_started;
  bool                                        m_write_in_progress; // internal only, doesn't need to be atomic
  chops::net::detail::output_queue<Protocol>  m_outq;
  std::experimental::net::ip::basic_endpoint<Protocol>
                                              m_remote_endp;

public:

  io_common() noexcept : m_started(false), m_write_in_progress(false), m_outq(), m_remote_endp() { }

  chops::net::output_queue_stats get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  std::experimental::net::ip::basic_endpoint<Protocol> get_remote_endp() const noexcept { return m_remote_endp; }

  bool is_started() const noexcept { return m_started; }

  bool is_write_in_progress() const noexcept { return m_write_in_progress; }

  void stop() noexcept { m_started = false; m_write_in_progress = false; }

  bool start_io_setup(std::experimental::net::basic_socket<Protocol>&) noexcept;

  // assumption - following methods called from single thread only
  bool start_write_setup(const chops::const_shared_buffer&);
  bool start_write_setup(const chops::const_shared_buffer&, 
                         const std::experimental::net::ip::basic_endpoint<Protocol>&);

  typename output_queue<Protocol>::opt_queue_element get_next_element();

};

template <typename Protocol>
bool io_common<Protocol>::start_io_setup(std::experimental::net::basic_socket<Protocol>& sock) noexcept {
  if (m_started) {
    return false;
  }
  m_started = true;
  std::error_code ec;
  m_remote_endp = sock.remote_endpoint(ec); // if fails, remote endp is left as default constructed
  return true;
}

template <typename Protocol>
bool io_common<Protocol>::start_write_setup(const chops::const_shared_buffer& buf) {
  if (!m_started) {
    return false; // shutdown happening, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(buf);
    return false;
  }
  m_write_in_progress = true;
  return true;
}

template <typename Protocol>
bool io_common<Protocol>::start_write_setup(const chops::const_shared_buffer& buf,
                                            const std::experimental::net::ip::basic_endpoint<Protocol>& endp) {
  if (!m_started) {
    return false; // shutdown happening, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(buf, endp);
    return false;
  }
  m_write_in_progress = true;
  return true;
}

template <typename Protocol>
typename output_queue<Protocol>::opt_queue_element io_common<Protocol>::get_next_element() {
  if (!m_started) { // shutting down
    return typename output_queue<Protocol>::opt_queue_element { };
  }
  auto elem = m_outq.get_next_element();
  m_write_in_progress = elem.has_value();
  return elem;
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

