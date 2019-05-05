/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the mock classes shared between @c net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t
#include <vector>
#include <algorithm>
#include <memory> // std::shared_ptr, std::make_shared
#include <future>

#include "asio/ip/udp.hpp"
#include "asio/buffer.hpp"

#include "utility/make_byte_array.hpp"
#include "marshall/shared_buffer.hpp"

#include "net_ip_component/simple_variable_len_msg_frame.hpp"

#include "net_ip/mock_classes_test.hpp"

SCENARIO ( "Mock classes testing, io_handler_mock test",
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

  auto t = io_mock.mock_sock;

  GIVEN ("A default constructed io_handler_mock") {
    WHEN ("is_io_started is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (io_mock.is_io_started());
      }
    }
    AND_WHEN ("visit_socket is called") {
      THEN ("the correct value is set") {
        io_mock.visit_socket([] (double& d) { d += 2.0; });
        REQUIRE (io_mock.mock_sock == 44.0);
      }
    }
    AND_WHEN ("get_output_queue_stats is called") {
      THEN ("the correct value is returned") {
        auto qs = io_mock.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == io_mock.qs_base);
        REQUIRE (qs.bytes_in_output_queue == (io_mock.qs_base+1));
      }
    }
    AND_WHEN ("the start_io methods are called") {
      THEN ("is_io_started and related flags are true") {
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
      }
    }
    AND_WHEN ("stop_io is called") {
      THEN ("is_io_started is false") {
        io_mock.start_io(std::string_view(), [] { });
        REQUIRE (io_mock.is_io_started());
        io_mock.stop_io();
        REQUIRE_FALSE (io_mock.is_io_started());
      }
    }
  } // end given
}

