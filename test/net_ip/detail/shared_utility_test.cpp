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
#include <vector>
#include <algorithm>
#include <memory> // std::shared_ptr, std::make_shared

#include <boost/endian/conversion.hpp>

#include <experimental/buffer>
#include <experimental/internet>

#include "utility/make_byte_array.hpp"
#include "utility/shared_buffer.hpp"
#include "net_ip/io_interface.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"

namespace chops {
namespace test {

chops::mutable_shared_buffer make_body_buf(std::string_view pre, char body_char, std::size_t num_body_chars) {
  chops::mutable_shared_buffer buf(pre.data(), pre.size());
  std::string body(num_body_chars, body_char);
  return buf.append(body.data(), body.size());
}

chops::mutable_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body) {
  std::uint16_t hdr = boost::endian::native_to_big(static_cast<std::uint16_t>(body.size()));
  chops::mutable_shared_buffer msg(static_cast<const void*>(&hdr), 2);
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


std::size_t decode_variable_len_msg_hdr(const std::byte* buf_ptr) {
  std::uint16_t hdr;
  std::byte* hdr_ptr = static_cast<std::byte*>(static_cast<void*>(&hdr));
  *(hdr_ptr+0) = *(buf_ptr+0);
  *(hdr_ptr+1) = *(buf_ptr+1);
  return boost::endian::big_to_native(hdr);
}

} // end namespace test
} // end namespace chops

void make_msg_test() {
  using namespace chops::test;

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

    AND_WHEN ("a larger buffer is passed to make_variable_len_msg") {
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

template <typename F>
void make_msg_set_test(F&& f) {
  using namespace chops::test;

  GIVEN ("A preamble and a char to repeat") {
    auto empty = make_empty_body_msg(f);
    int delta = empty.size();
    REQUIRE (delta <= 2);
    WHEN ("make_msg_set is called") {
      auto vb = make_msg_set(f, "Good tea!", 'Z', 20);
      THEN ("a vector of buffers is returned") {
        REQUIRE (vb.size() == 20);
        chops::repeat(20, [&vb, delta] (const int& i) { REQUIRE (vb[i].size() == (i+10+delta)); } );
      }
    }
  } // end given
}

SCENARIO ( "Shared Net IP test utility, make msg",
           "[shared_utility] [make_msg]" ) {

  make_msg_test();

}

SCENARIO ( "Shared Net IP test utility, make msg set",
           "[shared_utility] [make_msg_set]" ) {
  using namespace chops::test;

  make_msg_set_test(make_variable_len_msg);
  make_msg_set_test(make_cr_lf_text_msg);
  make_msg_set_test(make_lf_text_msg);

}

SCENARIO ( "Shared Net IP test utility, decode variable len msg header",
           "[shared_utility] [decode_variable_len_msg]" ) {
  using namespace chops::test;
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x02, 0x01); // 513 in big endian

  GIVEN ("A two byte buffer that is a variable len msg header") {
    WHEN ("the decode variable len msg hdr function is called") {
      THEN ("the correct length is returned") {
        REQUIRE(decode_variable_len_msg_hdr(ba.data()) == 513);
      }
    }
    AND_WHEN ("a simple variable len msg frame is constructed") {
      mutable_buffer buf(ba.data(), ba.size());
      auto mf = chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr);
      THEN ("the returned length toggles between the decoded length and zero") {
        REQUIRE(mf(buf) == 513);
        REQUIRE(mf(buf) == 0);
        REQUIRE(mf(buf) == 513);
        REQUIRE(mf(buf) == 0);
      }
    }
  } // end given
}

SCENARIO ( "Shared Net IP test utility, msg hdlr",
           "[shared_utility] [msg_hdlr]" ) {
  using namespace chops::test;
  using namespace std::experimental::net;

  struct ioh_mock {
    using endpoint_type = std::experimental::net::ip::tcp::endpoint;
    using socket_type = std::experimental::net::ip::tcp::socket;

    bool send_called = false;

    void send(chops::const_shared_buffer) { send_called = true; }
  };

  auto iohp = std::make_shared<ioh_mock>();
  REQUIRE_FALSE(iohp->send_called);
  ip::tcp::endpoint endp { };

  auto msg = make_variable_len_msg(make_body_buf("Bah, humbug!", 'T', 4));
  auto empty = make_variable_len_msg(chops::mutable_shared_buffer());

  GIVEN ("A mock io handler, a msg with a body, and an empty body msg") {

    WHEN ("a msg hdlr is created with reply true and the messages passed in") {
      vec_buf vb;
      msg_hdlr<ioh_mock> mh(vb, true);
      THEN ("send has been called, shutdown message is handled correctly and msg container size is correct") {
        REQUIRE(mh(const_buffer(msg.data(), msg.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE(iohp->send_called);
        REQUIRE(mh(const_buffer(empty.data(), empty.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE_FALSE(mh(const_buffer(empty.data(), empty.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE(vb.size() == 1);
      }
    }
    AND_WHEN ("a msg hdlr is created with reply false and the messages passed in") {
      vec_buf vb;
      msg_hdlr<ioh_mock> mh(vb, false);
      THEN ("shutdown message is handled correctly and msg container size is correct") {
        REQUIRE(mh(const_buffer(msg.data(), msg.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE(mh(const_buffer(empty.data(), empty.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE_FALSE(mh(const_buffer(empty.data(), empty.size()), chops::net::io_interface<ioh_mock>(iohp), endp));
        REQUIRE(vb.size() == 1);
      }
    }
  } // end given
}

