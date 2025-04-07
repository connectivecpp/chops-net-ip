/** @file
 *
 * @brief Test the simple variable length message framing functor.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2019-2025 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include "asio/buffer.hpp"

#include "utility/byte_array.hpp"

#include "net_ip/simple_variable_len_msg_frame.hpp"
#include "shared_test/msg_handling.hpp"

SCENARIO ( "Simple variable length message frame",
           "[simple_variable_len_msg_frame]" ) {
  using namespace chops::test;

  auto ba = chops::make_byte_array(0x02, 0x01); // 513 in big endian

  GIVEN ("A two byte buffer that is a variable len msg header") {
    WHEN ("the decode variable len msg hdr function is called") {
      THEN ("the correct length is returned") {
        REQUIRE(decode_variable_len_msg_hdr(ba.data(), 2) == 513);
      }
    }
    AND_WHEN ("a simple variable len msg frame is constructed") {
      asio::mutable_buffer buf(ba.data(), ba.size());
      chops::net::simple_variable_len_msg_frame mf(decode_variable_len_msg_hdr);
      THEN ("the returned length toggles between the decoded length and zero") {
        REQUIRE(mf(buf) == 513);
        REQUIRE(mf(buf) == 0);
        REQUIRE(mf(buf) == 513);
        REQUIRE(mf(buf) == 0);
      }
    }
  } // end given
}

