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
#include <functional> // std::function, for start and shutdown state change cb's
#include <memory>
#include <cstddef> // std::size_t

#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename IOH>
class net_entity_base {
public:
  using start_change_cb = std::function<void (io_interface<IOH>, std::size_t)>;
  using shutdown_change_cb = std::function<void (io_interface<IOH>, std::error_code, std::size_t)>;

private:
  std::atomic_bool           m_started; // may be called from multiple threads concurrently
  start_change_cb            m_start_change_cb;
  shutdown_change_cb         m_shutdown_change_cb;

public:

  net_entity_base() noexcept : m_started(false), m_start_change_cb(), m_shutdown_change_cb() { }

  // following three methods can be called concurrently
  bool is_started() const noexcept { return m_started; }

  template <typename R, typename S>
  bool start(R&& start_func, S&& shutdown_func) {
    bool expected = false;
    if (m_started.compare_exchange_strong(expected, true)) {
      m_start_change_cb = start_func;
      m_shutdown_change_cb = shutdown_func;
      return true;
    }
    return false;
  }

  bool stop() {
    bool expected = true;
    return m_started.compare_exchange_strong(expected, false); 
  }

  void call_start_change_cb(std::shared_ptr<IOH> p, std::size_t sz) {
    m_start_change_cb(io_interface<IOH>(p), sz);
  }

  void call_shutdown_change_cb(std::shared_ptr<IOH> p, const std::error_code& err, std::size_t sz) {
    m_shutdown_change_cb(io_interface<IOH>(p), err, sz);
  }


};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

