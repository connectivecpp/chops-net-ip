/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations and implementations for utility code shared between 
 *  @c net_ip tests.
 *
 *  The general Chops Net IP test strategy is to have message senders and message 
 *  receivers, with a flag specifying whether the receiver is to loop back the
 *  messages. For TCP it is independent of whether the sender or receiver is an 
 *  acceptor or connector, although most tests have the connector being a sender. In 
 *  the test routines, coordination is typically needed to know when a connection has 
 *  been made or sender / receiver is ready so that message flow can start. At the 
 *  higher layers, the Chops Net IP library facilities provide connection state
 *  change function object callbacks.
 *
 *  When the message flow is finished, an empty body message is sent to the receiver
 *  (and looped back if the reply flag is set), which signals an "end of message 
 *  flow" condition. The looped back empty message may not arrive back to the 
 *  sender since connections or handlers are in the process of being taken down.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef SHARED_UTILITY_TEST_HPP_INCLUDED
#define SHARED_UTILITY_TEST_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t
#include <vector>
#include <utility> // std::forward, std::move
#include <atomic>
#include <memory> // std::shared_ptr
#include <thread>
#include <system_error>

#include <cassert>
#include <limits>

#include "asio/buffer.hpp"
#include "asio/ip/udp.hpp" // ip::udp::endpoint
#include "asio/ip/address.hpp" // make_address

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"
#include "utility/cast_ptr_to.hpp"

#include "marshall/extract_append.hpp"
#include "marshall/shared_buffer.hpp"

#include "net_ip/io_interface.hpp"
#include "net_ip/io_output.hpp"

namespace chops {
namespace test {

struct io_handler_mock {
  using endpoint_type = asio::ip::udp::endpoint;

  bool started = false;
  constexpr static std::size_t qs_base = 42;

  bool is_io_started() const { return started; }

  socket_type& get_socket() { return sock; }

  chops::net::output_queue_stats get_output_queue_stats() const { 
    return chops::net::output_queue_stats { qs_base, qs_base +1 };
  }

  bool send_called = false;

  void send(chops::const_shared_buffer) { send_called = true; }
  void send(chops::const_shared_buffer, const endpoint_type&) { send_called = true; }

  bool mf_sio_called = false;
  bool delim_sio_called = false;
  bool rd_sio_called = false;
  bool rd_endp_sio_called = false;
  bool send_sio_called = false;
  bool send_endp_sio_called = false;

  template <typename MH, typename MF>
  bool start_io(std::size_t, MH&&, MF&&) {
    return started ? false : started = true, mf_sio_called = true, true;
  }

  template <typename MH>
  bool start_io(std::string_view, MH&&) {
    return started ? false : started = true, delim_sio_called = true, true;
  }

  template <typename MH>
  bool start_io(std::size_t, MH&&) {
    return started ? false : started = true, rd_sio_called = true, true;
  }

  template <typename MH>
  bool start_io(const endpoint_type&, std::size_t, MH&&) {
    return started ? false : started = true, rd_endp_sio_called = true, true;
  }

  bool start_io() {
    return started ? false : started = true, send_sio_called = true, true;
  }

  bool start_io(const endpoint_type&) {
    return started ? false : started = true, send_endp_sio_called = true, true;
  }

  bool stop_io() {
    return started ? started = false, true : false;
  }

};

using io_handler_mock_ptr = std::shared_ptr<io_handler_mock>;
using io_interface_mock = chops::net::basic_io_interface<io_handler_mock>;

struct net_entity_mock {

  io_handler_mock_ptr  iop;
  std::thread          thr;
  
  net_entity_mock() : iop(std::make_shared<io_handler_mock>()) { }

  using socket_type = double;
  using endpoint_type = int;

  constexpr static double special_val = 42.0;
  double dummy = special_val;

  bool started = false;

  bool is_started() const { return started; }

  double& get_socket() { return dummy; }

  template <typename F1, typename F2>
  bool start(F1&& io_state_chg_func, F2&& err_func ) {
    if (started) {
      return false;
    }
    started = true;
    thr = std::thread([ this, ios = std::move(io_state_chg_func), ef = std::move(err_func)] () mutable {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ios(io_interface_mock(iop), 1, true);
        ef(io_interface_mock(iop), 
                 std::make_error_code(chops::net::net_ip_errc::message_handler_terminated));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ios(io_interface_mock(iop), 0, false);
      }
    );
    return true;
  }

  bool stop() {
    if (!started) {
      return false;
    }
    started = false;
    join_thr();
    return true;
  }

  void join_thr() { thr.join(); }

};

inline void io_state_chg_mock(io_interface_mock, std::size_t, bool) { }
inline void err_func_mock(io_interface_mock, std::error_code) { }

asio::ip::udp::endpoint make_udp_endpoint(const char* addr, int port_num) {
  return asio::ip::udp::endpoint(asio::ip::make_address(addr),
                           static_cast<unsigned short>(port_num));
}


} // end namespace test
} // end namespace chops

#endif

