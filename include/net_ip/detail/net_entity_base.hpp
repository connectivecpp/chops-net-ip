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
#include <functional> // std::function, for state change cb
#include <memory>
#include <vector>
#include <cstddef> // std::size_t

#include <experimental/executor>

#include "net_ip/io_interface.hpp"
#include "utility/erase_where.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename IOH>
class net_entity_base {
public:
  using start_change_cb = std::function<void (io_interface<IOH>, std::size_t)>;
  using shutdown_change_cb = std::function<void (io_interface<IOH>, std::error_code, std::size_t)>;

private:
  using io_handlers = std::vector<std::shared_ptr<IOH> >;

private:

  std::atomic_bool           m_started; // may be called from multiple threads concurrently
  start_change_cb            m_start_change_cb;
  shutdown_change_cb         m_shutdown_change_cb;
  io_handlers                m_io_handlers;

public:

  net_entity_base() noexcept : m_started(false), 
         m_start_change_cb(), m_shutdown_change_cb(), m_io_handlers() { }

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



  // following methods are not thread-safe, use only from within run thread
  std::size_t size() const noexcept { return m_io_handlers.size(); }

  // careful - there is a coupling where the acceptor or connector or udp entity performs 
  // a remove_handler as part of the stop_io shutdown
  void stop_io_all() {
    auto hdlrs = m_io_handlers;
    for (auto i : hdlrs) {
      i->stop_io();
    }
  }

  void add_handler(std::shared_ptr<IOH> p) {
    m_io_handlers.push_back(p);
  }

  void remove_handler(std::shared_ptr<IOH> p) {
    chops::erase_where(m_io_handlers, p);
  }

  void clear_handlers() {
    m_io_handlers.clear();
  }

  void call_start_change_cb(std::shared_ptr<IOH> p) {
    m_start_change_cb(io_interface<IOH>(p), size());
  }

  void call_shutdown_change_cb(const std::error_code& err, std::shared_ptr<IOH> p) {
    m_shutdown_change_cb(io_interface<IOH>(p), err, size());
  }


};

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

