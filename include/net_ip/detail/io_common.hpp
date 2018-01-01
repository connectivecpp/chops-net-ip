/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP and UDP io handlers.
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
#include <functional> // std::function, used for type erased notifications to net_entity objects
#include <memory> // std::shared_ptr

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/buffer>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/queue_stats.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

inline std::size_t null_msg_frame (std::experimental::net::const_buffer /* buf */ ) {
  return 0;
}

template <typename IOH>
class io_common {
public:
  using entity_notifier_cb = std::function<void (const std::error_code&, std::shared_ptr<IOH>)>;
  using endpoint_type = std::experimental::net::ip::basic_endpoint<typename IOH::protocol_type>;
  using outq_type = output_queue<typename IOH::protocol_type>;
  using outq_opt_el = typename outq_type::opt_queue_element;
  using queue_stats = chops::net::output_queue_stats;

private:

  std::atomic_bool        m_started; // may be called from multiple threads concurrently
  bool                    m_write_in_progress; // internal only, doesn't need to be atomic
  outq_type               m_outq;
  endpoint_type           m_remote_endp;
  entity_notifier_cb      m_entity_notifier_cb;

public:

  explicit io_common(entity_notifier_cb cb) noexcept :
    m_started(false), m_write_in_progress(false), m_outq(), m_remote_endp(), m_entity_notifier_cb(cb) { }

  queue_stats get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  endpoint_type get_remote_endp() const noexcept { return m_remote_endp; }

  bool is_started() const noexcept { return m_started; }

  bool is_write_in_progress() const noexcept { return m_write_in_progress; }

  void stop() noexcept { m_started = false; m_write_in_progress = false; }

  bool start_io_setup(typename IOH::socket_type&) noexcept;

  bool check_err_code(const std::error_code&, std::shared_ptr<IOH>);

  // assumption - following methods called from single thread only
  bool start_write_setup(const chops::const_shared_buffer&);
  bool start_write_setup(const chops::const_shared_buffer&, const endpoint_type&);

  outq_opt_el get_next_element();

};

template <typename IOH>
bool io_common<IOH>::check_err_code(const std::error_code& err, std::shared_ptr<IOH> ioh_ptr) {
  if (err) {
    m_entity_notifier_cb(err, ioh_ptr);
    return false;
  }
  return true;
}

template <typename IOH>
bool io_common<IOH>::start_io_setup(typename IOH::socket_type& sock) noexcept {
  if (m_started) {
    return false;
  }
  m_started = true;
  std::error_code ec;
  m_remote_endp = sock.remote_endpoint(ec); // if fails, remote endp is left as default constructed
  return true;
}

template <typename IOH>
bool io_common<IOH>::start_write_setup(const chops::const_shared_buffer& buf) {
  if (!m_started) {
    return false; // shutdown happening or not started, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(buf);
    return false;
  }
  m_write_in_progress = true;
  return true;
}

template <typename IOH>
bool io_common<IOH>::start_write_setup(const chops::const_shared_buffer& buf, const endpoint_type& endp) {
  if (!m_started) {
    return false; // shutdown happening or not started, don't start a write
  }
  if (m_write_in_progress) { // queue buffer
    m_outq.add_element(buf, endp);
    return false;
  }
  m_write_in_progress = true;
  return true;
}

template <typename IOH>
typename io_common<IOH>::outq_opt_el io_common<IOH>::get_next_element() {
  if (!m_started) { // shutting down
    return outq_opt_el { };
  }
  auto elem = m_outq.get_next_element();
  m_write_in_progress = elem.has_value();
  return elem;
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

