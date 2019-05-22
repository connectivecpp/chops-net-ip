/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Mock classes shared between various @c chops_net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MOCK_CLASSES_HPP_INCLUDED
#define MOCK_CLASSES_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <system_error>

#include "asio/ip/udp.hpp" // ip::udp::endpoint

#include "marshall/shared_buffer.hpp"

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"
#include "net_ip/simple_variable_len_msg_frame.hpp"
#include "net_ip/net_ip_error.hpp"


namespace chops {
namespace test {

inline std::size_t mock_hdr_decoder_func (const std::byte*, std::size_t) { return 0u; }

struct io_handler_mock {
  using endpoint_type = asio::ip::udp::endpoint;

  double mock_sock = 42.0;

  bool started = false;
  constexpr static std::size_t qs_base = 42;

  bool is_io_started() const { return started; }

  template <typename F>
  void visit_socket(F&& f) {
    f(mock_sock);
  }

  chops::net::output_queue_stats get_output_queue_stats() const { 
    return chops::net::output_queue_stats { qs_base, qs_base +1 };
  }

  bool send_called = false;

  bool send(chops::const_shared_buffer) { send_called = true; return true; }
  bool send(chops::const_shared_buffer, const endpoint_type&) { send_called = true; return true; }

  bool mf_sio_called = false;
  bool simple_var_len_sio_called = false;
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
  bool start_io(std::size_t, MH&&, chops::net::hdr_decoder_func) {
    return started ? false : started = true, simple_var_len_sio_called = true, true;
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

struct net_entity_mock {

  io_handler_mock mock_ioh;

  float mock_sock = 11.0f;

  bool started = false;

  bool is_started() const { return started; }

  template <typename F>
  void visit_socket(F&& f) {
    f(mock_sock);
  }

  template <typename F>
  std::size_t visit_io_output(F&& f) {
    f(chops::net::basic_io_output<io_handler_mock>(&mock_ioh));
    return 1u;
  }

  template <typename F1, typename F2>
  std::error_code start(F1&&, F2&&) {
    if (started) {
      return std::make_error_code(std::errc::too_many_files_open);
    }
    started = true;
    return std::error_code {};
  }

  std::error_code stop() {
    if (!started) {
      return std::make_error_code(std::errc::too_many_files_open);
    }
    started = false;
    return std::error_code {};
  }

};

using io_interface_mock = chops::net::basic_io_interface<io_handler_mock>;
using io_output_mock = chops::net::basic_io_output<io_handler_mock>;

inline bool io_state_chg_mock(io_interface_mock, std::size_t, bool) { }
inline void err_func_mock(io_interface_mock, std::error_code) { }


} // end namespace test
} // end namespace chops

#endif

