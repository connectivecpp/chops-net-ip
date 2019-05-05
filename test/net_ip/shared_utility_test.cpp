/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the lower level utility code shared between @c net_ip tests.
 *
 *  The body of a msg is constructed of a preamble followed by a repeated 
 *  char. There are three forms of messages:
 *  1. Variable len: header is 16 bit big endian integer containing length of body
 *  2. Text, CR LF: body is followed by Ascii CR and LF chars
 *  3. Text, LF: body is followed by Ascii LF char
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
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

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip_component/simple_variable_len_msg_frame.hpp"

#include "net_ip/shared_utility_test.hpp"

SCENARIO ( "Shared Net IP test utility, net_entity_mock test",
           "[shared_utility] [net_entity_mock]" ) {
  using namespace chops::test;

  net_entity_mock ne_mock { };
  REQUIRE_FALSE (ne_mock.is_started());

  auto t = ne_mock.special_val;

  GIVEN ("A default constructed net_entity_mock") {
    WHEN ("is_started is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (ne_mock.is_started());
      }
    }
    AND_WHEN ("get_socket is called") {
      THEN ("the correct value is returned") {
        REQUIRE (ne_mock.get_socket() == t);
      }
    }
    AND_WHEN ("get_socket is used as a modifying call") {
      THEN ("the correct value is returned") {
        ne_mock.get_socket() += 1.0;
        REQUIRE (ne_mock.get_socket() == (t+1.0));
      }
    }
    AND_WHEN ("start and then stop is called") {
      THEN ("is_started is set appropriately") {
        ne_mock.start(io_state_chg_mock, err_func_mock);
        REQUIRE (ne_mock.is_started());
        ne_mock.stop();
        REQUIRE_FALSE (ne_mock.is_started());
      }
    }
  } // end given
}

