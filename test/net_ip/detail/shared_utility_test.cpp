/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Utility code shared between @c net_ip tests.
 *
 *  The body of a msg is constructed of a preamble followed by a repeated 
 *  char. There are three forms of messages:
 *  1. Variable len: header is 16 bit big endian integer containing length of body
 *  2. Text, CR LF: body is followed by Ascii CR and LF chars
 *  3. Text, LF: body is followed by Ascii LF char
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <string_view>
#include <string>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t

#include <boost/endian/buffers.hpp>

#include "utility/make_byte_array.hpp"
#include "utility/shared_buffer.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"

chops::mutable_shared_buffer make_body_buf(std::string_view pre, char body_char, std::size_t num_body_chars) {
  chops::mutable_shared_buffer buf(pre.data(), pre.size());
  std::string body(num_body_chars, body_char);
  return buf.append(body.data(), body.size());
}

chops::mutable_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body) {
  boost::endian::big_uint16_buf_t hdr; // no constructor that will take the val directly
  hdr = static_cast<std::uint16_t>(body.size());
  chops::mutable_shared_buffer msg(hdr.data(), 2);
  return msg.append(body.data(), body.size());
}

chops::mutable_shared_buffer make_cr_lf_text_msg(const chops::mutable_shared_buffer& body) {
  chops::mutable_shared_buffer msg(body.data(), body.size());
  auto ba = chops::make_byte_array(0x0D, 0x0A); // CR, LF
  return msg.append(ba.data(), ba.size());
}

chops::mutable_shared_buffer make_lf_text_msg(const chops::mutable_shared_buffer& body) {
  chops::mutable_shared_buffer msg(body.data(), body.size());
  auto ba = chops::make_byte_array(0x0A); // LF
  return msg.append(ba.data(), ba.size());
}


void make_msg_test() {

  GIVEN ("A body consisting of a preamble and a char to repeat") {

    auto body = make_body_buf("HappyNewYear!", 'Q', 10);
    REQUIRE (body.size() == 23);

    WHEN ("make_variable_len_msg is called") {
      auto msg = make_variable_len_msg(body);
      THEN ("the correct header is prepended") {
        REQUIRE (msg.size() == 25); // full size of msg
        REQUIRE (*(msg.data()+0) == std::byte(0x00));
        REQUIRE (*(msg.data()+1) == std::byte(0x17)); // header is 16 bits, value 23 in big endian
        REQUIRE (*(msg.data()+2) == std::byte(0x48)); // 'H' in ascii
        REQUIRE (*(msg.data()+3) == std::byte(0x61)); // 'a' in ascii
        REQUIRE (*(msg.data()+15) == std::byte(0x51)); // 'Q' in ascii
        REQUIRE (*(msg.data()+16) == std::byte(0x51)); // 'Q' in ascii
      }
    }

    AND_WHEN ("make_cr_lf_text_msg is called") {
      auto msg = make_cr_lf_text_msg(body);
      THEN ("CR and LF are appended") {
        REQUIRE (msg.size() == 25); // full size of msg
        REQUIRE (*(msg.data()+0) == std::byte(0x48)); // 'H' in ascii
        REQUIRE (*(msg.data()+1) == std::byte(0x61)); // 'a' in ascii
        REQUIRE (*(msg.data()+13) == std::byte(0x51)); // 'Q' in ascii
        REQUIRE (*(msg.data()+14) == std::byte(0x51)); // 'Q' in ascii
        REQUIRE (*(msg.data()+23) == std::byte(0x0D)); // CR in ascii
        REQUIRE (*(msg.data()+24) == std::byte(0x0A)); // LF in ascii
      }
    }

    AND_WHEN ("make_lf_text_msg is called") {
      auto msg = make_lf_text_msg(body);
      THEN ("LF is appended") {
        REQUIRE (msg.size() == 24); // full size of msg
        REQUIRE (*(msg.data()+0) == std::byte(0x48)); // 'H' in ascii
        REQUIRE (*(msg.data()+1) == std::byte(0x61)); // 'a' in ascii
        REQUIRE (*(msg.data()+13) == std::byte(0x51)); // 'Q' in ascii
        REQUIRE (*(msg.data()+14) == std::byte(0x51)); // 'Q' in ascii
        REQUIRE (*(msg.data()+23) == std::byte(0x0A)); // LF in ascii
      }
    }

    WHEN ("a larger buffer is passed to make_variable_len_msg") {
      auto body = make_body_buf("HappyNewYear!", 'Q', 500);
      REQUIRE (body.size() == 513);

      auto msg = make_variable_len_msg(body);

      THEN ("the correct header is prepended") {
        REQUIRE (msg.size() == 515); // full size of msg
        REQUIRE (*(msg.data()+0) == std::byte(0x02));
        REQUIRE (*(msg.data()+1) == std::byte(0x01)); // header is 16 bits, value 513 in big endian
      }
    }

  } // end given
}

SCENARIO ( "Shared Net IP test utility, make msg", "[shared_test_utility_make_msg]" ) {

  make_msg_test();

}

// More test scenarios to be added as additional networking tests are created

