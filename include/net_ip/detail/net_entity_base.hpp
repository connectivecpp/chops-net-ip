/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP acceptor, connector, and UDP net 
 *  entity handlers.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef NET_ENTITY_BASE_HPP_INCLUDED
#define NET_ENTITY_BASE_HPP_INCLUDED

#include <atomic>
#include <system_error>
#include <functional> // std::function, used for type erased notifications to net_entity objects
#include <memory> // std::shared_ptr, std::unique_ptr
#include <vector>

#include <experimental/executor>

#include "net_ip/net_entity.hpp"
#include "net_ip/endpoints_resolver.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename IOH>
class net_entity_base {
public:
  using state_change_cb = std::function<void (net_entity<IOH>, std::error_code, std::size_t)>;

private:
  using endpoints = std::vector<typename IOH::endpoint_type>;
  using io_handlers = std::vector<std::shared_ptr<IOH> >;

private:

  std::atomic_bool           m_started; // may be called from multiple threads concurrently
  state_change_cb            m_state_change_cb;
  bool                       m_resolved;
  endpoints_resolver         m_resolver;
  endpoints                  m_endpoints;
  io_handlers                m_io_handlers;

public:

  explicit net_entity_base(state_change_cb cb) noexcept :
    m_started(false), m_state_change_cb(cb), m_ { }

  bool is_started() const noexcept { return m_started; }

  void stop() noexcept { m_started = false; m_write_in_progress = false; }

  bool start_io_setup(typename IOH::socket_type&) noexcept;

  void process_err_code(const std::error_code&, std::shared_ptr<IOH>);

};

template <typename IOH>
void net_entity_base<IOH>::process_err_code(const std::error_code& err, std::shared_ptr<IOH> ioh_ptr) {
  if (err) {
    m_entity_notifier_cb(err, ioh_ptr);
  }
}

template <typename IOH>
bool net_entity_base<IOH>::start_io_setup(typename IOH::socket_type& sock) noexcept {
  if (m_started) {
    return false;
  }
  m_started = true;
  std::error_code ec;
  m_remote_endp = sock.remote_endpoint(ec); // if fails, remote endp is left as default constructed
  return true;
}

template <typename IOH>
bool net_entity_base<IOH>::start_write_setup(const chops::const_shared_buffer& buf) {
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
bool net_entity_base<IOH>::start_write_setup(const chops::const_shared_buffer& buf, 
                                       const typename IOH::endpoint_type& endp) {
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
typename net_entity_base<IOH>::outq_opt_el net_entity_base<IOH>::get_next_element() {
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

