/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the mock classes shared between @c net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t
#include <vector>
#include <algorithm>
#include <memory> // std::shared_ptr, std::make_shared
#include <future>

#include "asio/ip/udp.hpp"
#include "asio/buffer.hpp"

#include "utility/byte_array.hpp"
#include "buffer/shared_buffer.hpp"

#include "net_ip/simple_variable_len_msg_frame.hpp"

#include "shared_test/mock_classes.hpp"

chops::const_shared_buffer make_small_buf() {
  auto ba = chops::make_byte_array(0x40, 0x41, 0x42);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

TEST_CASE ( "Mock classes testing, io_handler_mock test",
            "[io_handler_mock]" ) {
  using namespace chops::test;

  io_handler_mock io_mock { };

  REQUIRE_FALSE (io_mock.mf_sio_called);
  REQUIRE_FALSE (io_mock.simple_var_len_sio_called);
  REQUIRE_FALSE (io_mock.delim_sio_called);
  REQUIRE_FALSE (io_mock.rd_sio_called);
  REQUIRE_FALSE (io_mock.rd_endp_sio_called);
  REQUIRE_FALSE (io_mock.send_sio_called);
  REQUIRE_FALSE (io_mock.send_endp_sio_called);

  REQUIRE_FALSE (io_mock.send_called);

  REQUIRE_FALSE (io_mock.is_io_started());
  io_mock.visit_socket([] (double& d) { d += 2.0; });
  REQUIRE (io_mock.mock_sock == 44.0);
  auto qs = io_mock.get_output_queue_stats();
  REQUIRE (qs.output_queue_size == io_mock.qs_base);
  REQUIRE (qs.bytes_in_output_queue == (io_mock.qs_base+1));
  io_mock.start_io(0, [] { }, [] { });
  REQUIRE (io_mock.is_io_started());
  REQUIRE (io_mock.mf_sio_called);
  io_mock.start_io(0, [] { }, mock_hdr_decoder_func);
  REQUIRE (io_mock.simple_var_len_sio_called);
  io_mock.start_io(std::string_view(), [] { });
  REQUIRE (io_mock.delim_sio_called);
  io_mock.start_io(0, [] { });
  REQUIRE (io_mock.rd_sio_called);
  io_mock.start_io(asio::ip::udp::endpoint(), 0, [] { });
  REQUIRE (io_mock.rd_endp_sio_called);
  io_mock.start_io();
  REQUIRE (io_mock.send_sio_called);
  io_mock.start_io(asio::ip::udp::endpoint());
  REQUIRE (io_mock.send_endp_sio_called);
  io_mock.send(make_small_buf());
  REQUIRE (io_mock.send_called);
  io_mock.start_io(std::string_view(), [] { });
  REQUIRE (io_mock.is_io_started());
  io_mock.stop_io();
  REQUIRE_FALSE (io_mock.is_io_started());
}

TEST_CASE ( "Mock classes testing, net_entity_mock test",
            "[net_entity_mock]" ) {
  using namespace chops::test;

  net_entity_mock ne_mock { };

  REQUIRE_FALSE (ne_mock.started);
  REQUIRE (ne_mock.mock_ioh_sp);
  REQUIRE_FALSE (ne_mock.mock_ioh_sp->send_called);

  REQUIRE_FALSE (ne_mock.is_started());
  ne_mock.visit_socket([] (float& v) { v += 2.0; });
  REQUIRE (ne_mock.mock_sock == 13.0f);
  ne_mock.visit_io_output( [] (io_output_mock ioh) { ioh.send(make_small_buf()); });
  REQUIRE (ne_mock.mock_ioh_sp->send_called);
  auto r = ne_mock.start(io_state_chg_mock, err_func_mock);
  REQUIRE_FALSE (r);
  REQUIRE (ne_mock.is_started());
  auto r1 = ne_mock.start(io_state_chg_mock, err_func_mock);
  REQUIRE (r1);
  auto r2 = ne_mock.stop();
  REQUIRE_FALSE (r2);
  REQUIRE_FALSE (ne_mock.is_started());
  r = ne_mock.stop();
  REQUIRE (r);
  INFO ("Error code is: " << r.message());
  r1 = ne_mock.start(io_state_chg_mock, err_func_mock);
  REQUIRE_FALSE (r1);
  r2 = ne_mock.start(io_state_chg_mock, err_func_mock);
  REQUIRE (r2);
  INFO ("Error code is: " << r2.message());
}

