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
 *  Copyright (c) 2018-2025 by Cliff Green
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
#include <iostream> // std::cerr
#include <ranges> // std::views::iota

#include "asio/ip/udp.hpp"
#include "asio/buffer.hpp"

#include "utility/byte_array.hpp"
#include "buffer/shared_buffer.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/queue_stats.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/mock_classes.hpp"

void make_msg_test() {
  using namespace chops::test;

  auto body = make_body_buf("HappyNewYear!", 'Q', 10);
  REQUIRE (body.size() == 23u);

  SECTION ("Call make_variable_len_msg") {
    auto msg = make_variable_len_msg(body);
    REQUIRE (msg.size() == 25u); // full size of msg
    REQUIRE (std::to_integer<int>(*(msg.data()+0)) == 0x00);
    REQUIRE (std::to_integer<int>(*(msg.data()+1)) == 0x17); // header is 16 bits, value 23 in big endian
    REQUIRE (std::to_integer<int>(*(msg.data()+2)) == 0x48); // 'H' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+3)) == 0x61); // 'a' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+15)) == 0x51); // 'Q' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+16)) == 0x51); // 'Q' in ascii
  }

  SECTION ("Call make_cr_lf_text_msg") {
    auto msg = make_cr_lf_text_msg(body);
    REQUIRE (msg.size() == 25u); // full size of msg
    REQUIRE (std::to_integer<int>(*(msg.data()+0)) == 0x48); // 'H' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+1)) == 0x61); // 'a' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+13)) == 0x51); // 'Q' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+14)) == 0x51); // 'Q' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+23)) == 0x0D); // CR in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+24)) == 0x0A); // LF in ascii
  }

  SECTION ("Call make_lf_text_msg") {
    auto msg = make_lf_text_msg(body);
    REQUIRE (msg.size() == 24u); // full size of msg
    REQUIRE (std::to_integer<int>(*(msg.data()+0)) == 0x48); // 'H' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+1)) == 0x61); // 'a' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+13)) == 0x51); // 'Q' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+14)) == 0x51); // 'Q' in ascii
    REQUIRE (std::to_integer<int>(*(msg.data()+23)) == 0x0A); // LF in ascii
  }

  SECTION ("Call make_variable_len_msg with a larger buf") {
    auto big_body = make_body_buf("HappyNewYear!", 'Q', 500);
    REQUIRE (big_body.size() == 513u);

    auto msg = make_variable_len_msg(big_body);
    REQUIRE (msg.size() == 515u); // full size of msg
    REQUIRE (std::to_integer<int>(*(msg.data()+0)) == 0x02);
    REQUIRE (std::to_integer<int>(*(msg.data()+1)) == 0x01); // header is 16 bits, value 513 in big endian
  }

}

template <typename F>
void make_msg_vec_test(F&& f) {
  using namespace chops::test;

  SECTION ("Msg vector testing with a preamble and a char to repeat") {
    auto empty = make_empty_body_msg(f);
    auto delta = empty.size();
    REQUIRE (delta <= 2u);
    auto vb = make_msg_vec(f, "Good tea!", 'Z', 20);
    REQUIRE (vb.size() == 20u);
    for (int i : std::views::iota(0, 20)) {
      REQUIRE (vb[i].size() == (i+10+delta));
    }
  }
}

template <typename F>
std::size_t msg_hdlr_stress_test(F&& f, std::string_view pre, char body_char, int num_msgs) {
  using namespace chops::test;

  auto msgs = make_msg_vec(f, pre, body_char, num_msgs);
  auto empty = make_empty_body_msg(f);

  auto ioh_sp = std::make_shared<io_handler_mock>();
  asio::ip::udp::endpoint endp { };

  test_counter cnt{0};
  msg_hdlr<io_handler_mock> mh(false, cnt);

  int m {0};
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

TEST_CASE ( "Message handling shared test utility, make msg",
            "[msg_handling] [make_msg]" ) {

  make_msg_test();

}

TEST_CASE ( "Message handling shared test utility, make msg vec",
            "[msg_handling] [make_msg_vec]" ) {
  using namespace chops::test;

  make_msg_vec_test(make_variable_len_msg);
  make_msg_vec_test(make_cr_lf_text_msg);
  make_msg_vec_test(make_lf_text_msg);

}

TEST_CASE ( "Message handling shared test utility, msg hdlr function object",
            "[msg_handling] [msg_hdlr]" ) {
  using namespace chops::test;

  auto ioh_sp = std::make_shared<io_handler_mock>();
  REQUIRE_FALSE(ioh_sp->send_called);
  asio::ip::udp::endpoint endp { };

  auto msg = make_variable_len_msg(make_body_buf("Bah, humbug!", 'T', 4));
  auto empty = make_empty_variable_len_msg();

  SECTION ("Create a msg handler with mock io, reply true when send is called") {
    test_counter cnt{0};
    msg_hdlr<io_handler_mock> mh(true, cnt);
    REQUIRE(mh(asio::const_buffer(msg.data(), msg.size()), io_output_mock(ioh_sp), endp));
    REQUIRE(ioh_sp->send_called);
    REQUIRE_FALSE(mh(asio::const_buffer(empty.data(), empty.size()), 
                  io_output_mock(ioh_sp), endp));
    REQUIRE(cnt == 1);
  }
  SECTION ("Create a msg hdlr with mock io, reply false and send is called") {
    test_counter cnt{0};
    msg_hdlr<io_handler_mock> mh(false, cnt);
    REQUIRE(mh(asio::const_buffer(msg.data(), msg.size()), io_output_mock(ioh_sp), endp));
    REQUIRE_FALSE(mh(asio::const_buffer(empty.data(), empty.size()), 
                  io_output_mock(ioh_sp), endp));
    REQUIRE(cnt == 1);
  }
}

TEST_CASE ( "Message handling shared test utility, msg hdlr function object async stress test",
            "[msg_handling] [msg_hdlr] [stress]" ) {
  using namespace chops::test;

  constexpr int sz1 = 2000;
  constexpr int sz2 = 3000;
  constexpr int sz3 = 8000;

  auto cnt1 = std::async(std::launch::async, msg_hdlr_stress_test_variable_len_msg, "Async fun var len msg", 'A', sz1);
  auto cnt2 = std::async(std::launch::async, msg_hdlr_stress_test_cr_lf_text_msg, "Ha, hilarity cr lf text msg", 'L', sz2);
  auto cnt3 = std::async(std::launch::async, msg_hdlr_stress_test_lf_text_msg, "Nom, nom lf text msg", 'M', sz3);
  REQUIRE(cnt1.get() == sz1);
  REQUIRE(cnt2.get() == sz2);
  REQUIRE(cnt3.get() == sz3);
}

TEST_CASE ( "Message handling shared test utility, output queue stats poll condition",
            "[msg_handling] [output_stats_cond]" ) {
  using namespace chops::test;

  poll_output_queue_cond cond(100, std::cerr);

  chops::net::output_queue_stats stats1 { 20u, 100u };
  chops::net::output_queue_stats stats2 { 0u, 0u };

  REQUIRE_FALSE(cond(stats1));
  REQUIRE(cond(stats2));

}

TEST_CASE ( "Message handling shared test utility, fixed size message handling",
            "[msg_handling] [fixed_size]" ) {
  using namespace chops::test;

  auto buf = make_fixed_size_buf();
  REQUIRE (buf.size() == fixed_size_buf_size);
  REQUIRE (std::to_integer<int>(*(buf.data()+0)) == 0xDE);
  REQUIRE (std::to_integer<int>(*(buf.data()+1)) == 0xAD);
  REQUIRE (std::to_integer<int>(*(buf.data()+2)) == 0xBE);
  REQUIRE (std::to_integer<int>(*(buf.data()+3)) == 0xEF);
  REQUIRE (std::to_integer<int>(*(buf.data()+32)) == 0xCC);

  auto vec = make_fixed_size_msg_vec(3);
  REQUIRE (vec.size() == 3u);

  auto ioh_sp = std::make_shared<io_handler_mock>();
  REQUIRE_FALSE(ioh_sp->send_called);
  asio::ip::udp::endpoint endp { };

  test_counter cnt{0};
  test_prom prom1;
  test_prom prom2;
  auto fut1 { prom1.get_future() };
  auto fut2 { prom2.get_future() };
  fixed_size_msg_hdlr<io_handler_mock> mh1(std::move(prom1), 5u, cnt);
  fixed_size_msg_hdlr<io_handler_mock> mh2(std::move(prom2), 4u, cnt);
  REQUIRE(mh1(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh1(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh2(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(cnt == 3);
  REQUIRE(mh1(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh2(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(cnt == 5);
  REQUIRE(mh1(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh1(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh2(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(mh2(asio::const_buffer(buf.data(), buf.size()), io_output_mock(ioh_sp), endp));
  REQUIRE(fut1.get() == 0u);
  REQUIRE(fut2.get() == 0u);
  REQUIRE(cnt == 9);
}

