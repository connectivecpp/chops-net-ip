/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code, factored out, for TCP acceptor, TCP connector, and UDP net 
 *  entity handlers.
 *
 *  The state machine for a net entity object is unstarted, started, stopped,
 *  currently implemented within this class by a @c std::atomic_int.
 *
 *  Once an entity is stopped it is not allowed to be started again. This may
 *  change in the future, but internal designs would have to change to allow
 *  each net entity to completely transition through all of its shutdown operations
 *  before it can be started again.
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

#ifndef NET_ENTITY_COMMON_HPP_INCLUDED
#define NET_ENTITY_COMMON_HPP_INCLUDED

#include "asio/executor.hpp"
#include "asio/post.hpp"

#include <atomic>
#include <system_error>
#include <functional> // std::function, for io state change and error callbacks
#include <utility> // std::move
#include <memory>
#include <cstddef> // std::size_t
#include <future>

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename IOT>
class net_entity_common {
public:
  using io_state_chg_cb = 
    std::function<void (basic_io_interface<IOT>, std::size_t, bool)>;
  using error_cb = 
    std::function<void (basic_io_interface<IOT>, std::error_code)>;

private:
  // use of atomic for m_started allows multiple threads to call concurrently
  std::atomic_int      m_started; // 0 - unstarted, 1 - started, 2 - stopped
  io_state_chg_cb      m_io_state_chg_cb;
  error_cb             m_error_cb;

public:

  net_entity_common() noexcept : m_started(0), m_io_state_chg_cb(), m_error_cb() { }

  // following three methods can be called concurrently
  bool is_started() const noexcept { return m_started == 1; }

  bool is_stopped() const noexcept { return m_started == 2; }

  // set_stopped is needed for when the net entity closes itself
  // internally, due to an error or IO state change false return or
  // similar internal reason
  void set_stopped() noexcept { m_started = 2; }

  template <typename F1, typename F2, typename SF>
  std::error_code start(F1&& io_state_chg_func, F2&& err_func, 
                        const asio::executor& exec,
                        SF&& start_func) {
    int expected = 0;
    if (!m_started.compare_exchange_strong(expected, 1)) {
      return std::make_error_code(net_ip_errc::net_entity_already_started);
    }
    m_io_state_chg_cb = io_state_chg_func;
    m_error_cb = err_func;
    std::promise<std::error_code> prom;
    auto fut = prom.get_future();
    // start network entity processing in context of executor thread
    asio::post(exec, [&start_func, p = std::move(prom)] () mutable {
        p.set_value(start_func());
      }
    );
    return fut.get();
  }
 

  template <typename SF>
  std::error_code stop(const asio::executor& exec,
                       SF&& stop_func) {
    int expected = 1;
    if (!m_started.compare_exchange_strong(expected, 2)) {
      return std::make_error_code(net_ip_errc::net_entity_already_stopped);
    }
    std::promise<std::error_code> prom;
    auto fut = prom.get_future();
    // start closing in context of executor thread
    asio::post(exec, [&stop_func, p = std::move(prom)] () mutable {
        p.set_value(stop_func());
      }
    );
    return fut.get();
  }

  void call_io_state_chg_cb(std::shared_ptr<IOT> p, std::size_t sz, bool starting) {
    m_io_state_chg_cb(basic_io_interface<IOT>(p), sz, starting);
  }

  void call_error_cb(std::shared_ptr<IOT> p, const std::error_code& err) {
    m_error_cb(basic_io_interface<IOT>(p), err);
  }


};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

