/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the message handling utility test code shared between @c net_ip tests.
 *
 *  The body of a msg is constructed of a preamble followed by a repeated 
 *  char. There are three forms of messages:
 *  1. Variable len: header is 16 bit big endian integer containing length of body
 *  2. Text, CR LF: body is followed by Ascii CR and LF chars
 *  3. Text, LF: body is followed by Ascii LF char
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
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
#include <iostream> // std::cerr

#include "asio/ip/udp.hpp"
#include "asio/buffer.hpp"

#include "utility/make_byte_array.hpp"
#include "marshall/shared_buffer.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/queue_stats.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/mock_classes.hpp"

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
void make_msg_vec_test(F&& f) {
  using namespace chops::test;

  GIVEN ("A preamble and a char to repeat") {
    auto empty = make_empty_body_msg(f);
    int delta = empty.size();
    REQUIRE (delta <= 2);
    WHEN ("make_msg_vec is called") {
      auto vb = make_msg_vec(f, "Good tea!", 'Z', 20);
      THEN ("a vector of buffers is returned") {
        REQUIRE (vb.size() == 20);
        chops::repeat(20, [&vb, delta] (int i) { REQUIRE (vb[i].size() == (i+10+delta)); } );
      }
    }
  } // end given
}

template <typename F>
std::size_t msg_hdlr_stress_test(F&& f, std::string_view pre, char body_char, int num_msgs) {
  using namespace chops::test;

  auto msgs = make_msg_vec(f, pre, body_char, num_msgs);
  auto empty = make_empty_body_msg(f);

  auto ioh_sp = std::make_shared<io_handler_mock>();
  asio::ip::udp::endpoint endp { };

  test_counter cnt(0);
  msg_hdlr<io_handler_mock> mh(false, cnt);

  int m = 0;
  for (auto i : msgs) {
    auto ret = mh(asio::const_buffer(i.data(), i.size()), io_output_mock(ioh_sp), endp);
    if (++m % 1000 == 0) {
      REQUIRE(ret);
    }
  }
  REQUIRE_FALSE(mh(asio::const_buffer(empty.data(), empty.size()), io_output_mock(ioh_sp), endp));

  return cnt.load();

}

std::size_t msg_hdlr_stress_test_variable_len_msg(std::string_view pre, char body_char, int num_msgs) {
  return msg_hdlr_stress_test(chops::test::make_variable_len_msg, pre, body_char, num_msgs);
}
std::size_t msg_hdlr_stress_test_cr_lf_text_msg(std::string_view pre, char body_char, int num_msgs) {
  return msg_hdlr_stress_test(chops::test::make_cr_lf_text_msg, pre, body_char, num_msgs);
}
std::size_t msg_hdlr_stress_test_lf_text_msg(std::string_view pre, char body_char, int num_msgs) {
  return msg_hdlr_stress_test(chops::test::make_lf_text_msg, pre, body_char, num_msgs);
}

SCENARIO ( "Message handling shared test utility, make msg",
           "[msg_handling] [make_msg]" ) {

  make_msg_test();

}

SCENARIO ( "Message handling shared test utility, make msg vec",
           "[msg_handling] [make_msg_vec]" ) {
  using namespace chops::test;

  make_msg_vec_test(make_variable_len_msg);
  make_msg_vec_test(make_cr_lf_text_msg);
  make_msg_vec_test(make_lf_text_msg);

}

SCENARIO ( "Message handling shared test utility, msg hdlr function object",
           "[msg_handling] [msg_hdlr]" ) {
  using namespace chops::test;

  auto ioh_sp = std::make_shared<io_handler_mock>();
  REQUIRE_FALSE(ioh_sp->send_called);
  asio::ip::udp::endpoint endp { };

  auto msg = make_variable_len_msg(make_body_buf("Bah, humbug!", 'T', 4));
  auto empty = make_empty_variable_len_msg();

  GIVEN ("A mock io handler, a msg with a body, and an empty body msg") {

    WHEN ("a msg hdlr is created with reply true and send is called") {
      test_counter cnt(0);
      msg_hdlr<io_handler_mock> mh(true, cnt);
      THEN ("the shutdown message is handled correctly and count is correct") {
        REQUIRE(mh(asio::const_buffer(msg.data(), msg.size()), io_output_mock(ioh_sp), endp));
        REQUIRE(ioh_sp->send_called);
        REQUIRE_FALSE(mh(asio::const_buffer(empty.data(), empty.size()), 
                      io_output_mock(ioh_sp), endp));
        REQUIRE(cnt == 1);
      }
    }
    AND_WHEN ("a msg hdlr is created with reply false and send is called") {
      test_counter cnt(0);
      msg_hdlr<io_handler_mock> mh(false, cnt);
      THEN ("the shutdown message is handled correctly and count is correct") {
        REQUIRE(mh(asio::const_buffer(msg.data(), msg.size()), io_output_mock(ioh_sp), endp));
        REQUIRE_FALSE(mh(asio::const_buffer(empty.data(), empty.size()), 
                      io_output_mock(ioh_sp), endp));
        REQUIRE(cnt == 1);
      }
    }
  } // end given
}

SCENARIO ( "Message handling shared test utility, msg hdlr function object async stress test",
           "[msg_handling] [msg_hdlr] [stress]" ) {
  using namespace chops::test;

  constexpr int sz1 = 2000;
  constexpr int sz2 = 3000;
  constexpr int sz3 = 8000;

  GIVEN ("Three types of messages and three large sizes") {
    WHEN ("a msg hdlr is created and stress tested in separate async calls") {
      auto cnt1 = std::async(std::launch::async, msg_hdlr_stress_test_variable_len_msg, "Async fun var len msg", 'A', sz1);
      auto cnt2 = std::async(std::launch::async, msg_hdlr_stress_test_cr_lf_text_msg, "Ha, hilarity cr lf text msg", 'L', sz2);
      auto cnt3 = std::async(std::launch::async, msg_hdlr_stress_test_lf_text_msg, "Nom, nom lf text msg", 'M', sz3);

      THEN ("the three return values match") {
        REQUIRE(cnt1.get() == sz1);
        REQUIRE(cnt2.get() == sz2);
        REQUIRE(cnt3.get() == sz3);
      }
    }
  } // end given
}

SCENARIO ( "Message handling shared test utility, output queue stats poll condition",
           "[msg_handling] [output_stats_cond]" ) {
  using namespace chops::test;

  poll_output_queue_cond cond(100, std::cerr);

  chops::net::output_queue_stats stats1 { 20u, 100u };
  chops::net::output_queue_stats stats2 { 0u, 0u };

  REQUIRE_FALSE(cond(stats1));
  REQUIRE(cond(stats2));

}

SCENARIO ( "Message handling shared test utility, fixed size message handling",
           "[msg_handling] [fixed_size]" ) {
  using namespace chops::test;

  auto buf = make_fixed_size_buf();
  REQUIRE (buf.size() == fixed_size_buf_size);
  REQUIRE (*(buf.data()+0) == static_cast<std::byte>(0xDE));
  REQUIRE (*(buf.data()+1) == static_cast<std::byte>(0xAD));
  REQUIRE (*(buf.data()+2) == static_cast<std::byte>(0xBE));
  REQUIRE (*(buf.data()+3) == static_cast<std::byte>(0xEF));
  REQUIRE (*(buf.data()+32) == static_cast<std::byte>(0xCC));

  auto vec = make_fixed_size_msg_vec(3);
  REQUIRE (vec.size() == 3u);

  auto ioh_sp = std::make_shared<io_handler_mock>();
  REQUIRE_FALSE(ioh_sp->send_called);
  asio::ip::udp::endpoint endp { };

  GIVEN ("A mock io handler and a fixed size msg with a body") {

    WHEN ("a fixed size msg hdlr is created and msg hdlr invoked") {
      test_counter cnt(0);
      test_prom prom;
      auto fut = prom.get_future();
      fixed_size_msg_hdlr<io_handler_mock> mh(std::move(prom), 5, cnt);
      THEN ("the count is incremented correctly and a promise is satisfied") {
        REQUIRE(mh(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
        REQUIRE(cnt == 1);
        REQUIRE(mh(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
        REQUIRE(cnt == 2);
        REQUIRE(mh(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
        REQUIRE(mh(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
        REQUIRE(mh(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
        auto s = fut.get();
        REQUIRE (s == 5u);
      }
    }
  } // end given
}

