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

#ifndef NET_ENTITY_COMMON_HPP_INCLUDED
#define NET_ENTITY_COMMON_HPP_INCLUDED

#include <atomic>
#include <system_error>
#include <functional> // std::function, for shutdown state change cb
#include <utility> // std::move
#include <memory>
#include <cstddef> // std::size_t

#include "net_ip/basic_io_interface.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename IOH>
class net_entity_common {
public:
  using shutdown_change_cb = 
    std::function<void (basic_io_interface<IOH>, std::error_code, std::size_t)>;

private:
  std::atomic_bool           m_started; // may be called from multiple threads concurrently
  shutdown_change_cb         m_shutdown_change_cb;

public:

  net_entity_common() noexcept : m_started(false), m_shutdown_change_cb() { }

  // following four methods can be called concurrently
  bool is_started() const noexcept { return m_started; }

  template <typename F>
  bool start(F&& shutdown_func) {
    bool expected = false;
    if (m_started.compare_exchange_strong(expected, true)) {
      m_shutdown_change_cb = shutdown_func;
      return true;
    }
    return false;
  }

  bool start() {
    bool expected = false;
    return m_started.compare_exchange_strong(expected, true);
  }

  bool stop() {
    bool expected = true;
    return m_started.compare_exchange_strong(expected, false); 
  }

  void call_shutdown_change_cb(std::shared_ptr<IOH> p, const std::error_code& err, std::size_t sz) {
    if (m_shutdown_change_cb) {
      m_shutdown_change_cb(basic_io_interface<IOH>(p), err, sz);
    }
  }


};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

