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

#include "net_ip/simple_variable_len_msg_frame.hpp"

#include "shared_test/mock_classes.hpp"

chops::const_shared_buffer make_small_buf() {
  auto ba = chops::make_byte_array(0x40, 0x41, 0x42);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

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

  REQUIRE_FALSE (io_mock.send_called);

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
    AND_WHEN ("send is called") {
      THEN ("the send flag is set true") {
        io_mock.send(make_small_buf());
        REQUIRE (io_mock.send_called);
      }
    }
    AND_WHEN ("stop_io is called after start_io") {
      THEN ("is_io_started is false") {
        io_mock.start_io(std::string_view(), [] { });
        REQUIRE (io_mock.is_io_started());
        io_mock.stop_io();
        REQUIRE_FALSE (io_mock.is_io_started());
      }
    }
  } // end given
}

SCENARIO ( "Mock classes testing, net_entity_mock test",
           "[net_entity_mock]" ) {
  using namespace chops::test;

  net_entity_mock ne_mock { };

  REQUIRE_FALSE (ne_mock.started);
  REQUIRE (ne_mock.pause == 0);
  REQUIRE (ne_mock.mock_ioh_sp);
  REQUIRE_FALSE (ne_mock.mock_ioh_sp->send_called);

  GIVEN ("A default constructed net_entity_mock") {
    WHEN ("is_started is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (ne_mock.is_started());
      }
    }
    AND_WHEN ("visit_socket is called") {
      THEN ("the correct value is set") {
        ne_mock.visit_socket([] (float& v) { v += 2.0; });
        REQUIRE (ne_mock.mock_sock == 13.0f);
      }
    }
    AND_WHEN ("visit_io_output is called") {
      THEN ("the correct flag is set") {
        ne_mock.visit_io_output(
            [] (io_output_mock ioh) {
                   ioh.send(make_small_buf());
               }
        );
        REQUIRE (ne_mock.mock_ioh_sp->send_called);
      }
    }
    AND_WHEN ("the start method is called") {
      THEN ("is_started flag is true") {
        auto r = ne_mock.start(io_state_chg_mock, err_func_mock);
        REQUIRE_FALSE (r);
        REQUIRE (ne_mock.is_started());
      }
    }
    AND_WHEN ("stop is called after start") {
      THEN ("is_started is false") {
        auto r1 = ne_mock.start(io_state_chg_mock, err_func_mock);
        REQUIRE_FALSE (r1);
        auto r2 = ne_mock.stop(10);
        REQUIRE_FALSE (r2);
        REQUIRE_FALSE (ne_mock.is_started());
        REQUIRE (ne_mock.pause == 10);
      }
    }
    AND_WHEN ("stop is called before start") {
      THEN ("an error is returned") {
        auto r = ne_mock.stop(15);
        REQUIRE (r);
        INFO ("Error code is: " << r.message());
        REQUIRE (ne_mock.pause == 15);
      }
    }
    AND_WHEN ("start is called twice") {
      THEN ("an error is returned") {
        auto r1 = ne_mock.start(io_state_chg_mock, err_func_mock);
        REQUIRE_FALSE (r1);
        auto r2 = ne_mock.start(io_state_chg_mock, err_func_mock);
        REQUIRE (r2);
        INFO ("Error code is: " << r2.message());
      }
    }
  } // end given
}

