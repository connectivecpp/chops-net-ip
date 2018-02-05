/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c make_simple_variable_len_msg_frame.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/buffer>

#include <cstddef> // std::size_t, std::byte

#include "utility/make_byte_array.hpp"

using namespace std::experimental::net;

std::size_t decoder_func (const std::byte* buf, std::size_t sz) {
  REQUIRE (sz == 1);
  return *buf;
}

void simple_var_len_msg_frame_test (mutable_buffer buf, std::size_t expected_body_len) {

  chops::net::simple_variable_len_msg_frame mf(decoder_func);

  GIVEN ("A simple message frame object constructed with a decoder func") {
    auto a = mf; // verify copying the lambda does the right thing
    WHEN ("it is called multiple times") {
      THEN ("the return value toggles between the decoder supplied number and zero") {
        REQUIRE (mf(buf) == 1);
        REQUIRE (mf(buf) == 0);
        REQUIRE (mf(buf) == magic);
        REQUIRE (mf(buf) == 0);
        auto b = mf;
        REQUIRE (b(buf) == magic);
        REQUIRE (b(buf) == 0);

      }
    }
  } // end given
}

SCENARIO ( "Simple variable len message frame test",
           "[msg_frame]" ) {

  // mutable_buffer buf;

  auto msg1 = chops::make_byte_array(0x01, 0xBB);
  auto msg2 = chops::make_byte_array(0x03, 0xAA, 0xDD, 0xEE);
  auto msg3 = chops::make_byte_array(0x04, 0xDE, 0xAD, 0xBE, 0xEF);
}

