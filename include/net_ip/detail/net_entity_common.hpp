/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP acceptor, TCP connector, and UDP net 
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
#include <functional> // std::function, for io state change and error callbacks
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
  using io_state_chg_cb = 
    std::function<void (basic_io_interface<IOH>, std::size_t, bool)>;
  using error_cb = 
    std::function<void (basic_io_interface<IOH>, std::error_code)>;

private:
  std::atomic_bool     m_started; // may be called from multiple threads concurrently
  io_state_chg_cb      m_io_state_chg_cb;
  error_cb             m_error_cb;

public:

  net_entity_common() noexcept : m_started(false), m_io_state_chg_cb(), m_error_cb() { }

  // following three methods can be called concurrently
  bool is_started() const noexcept { return m_started; }

  template <typename F1, typename F2>
  bool start(F1&& io_state_chg_func, F2&& err_func) {
    bool expected = false;
    if (m_started.compare_exchange_strong(expected, true)) {
      m_io_state_chg_cb = io_state_chg_func;
      m_error_cb = err_func;
      return true;
    }
    return false;
  }

  bool stop() {
    bool expected = true;
    return m_started.compare_exchange_strong(expected, false); 
  }

  void call_io_state_chg_cb(std::shared_ptr<IOH> p, std::size_t sz, bool starting) {
    m_io_state_chg_cb(basic_io_interface<IOH>(p), sz, starting);
  }

  void call_error_cb(std::shared_ptr<IOH> p, const std::error_code& err) {
    m_error_cb(basic_io_interface<IOH>(p), err);
  }


};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

