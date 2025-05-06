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

TEST_CASE ( "Simple variable length message frame",
            "[simple_variable_len_msg_frame]" ) {
  using namespace chops::test;

  auto ba = chops::make_byte_array(0x02, 0x01); // 513 in big endian

  REQUIRE(decode_variable_len_msg_hdr(ba.data(), 2) == 513);
  asio::mutable_buffer buf(ba.data(), ba.size());
  chops::net::simple_variable_len_msg_frame mf(decode_variable_len_msg_hdr);
  REQUIRE(mf(buf) == 513);
  REQUIRE(mf(buf) == 0);
  REQUIRE(mf(buf) == 513);
  REQUIRE(mf(buf) == 0);
}

