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
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t
#include <vector>
#include <algorithm>
#include <memory> // std::shared_ptr, std::make_shared
#include <future>

#include <experimental/buffer>
#include <experimental/internet>

#include "utility/make_byte_array.hpp"
#include "utility/shared_buffer.hpp"

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip/component/simple_variable_len_msg_frame.hpp"

#include "net_ip/shared_utility_test.hpp"

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
  using namespace std::experimental::net;

  auto msgs = make_msg_vec(f, pre, body_char, num_msgs);
  auto empty = make_empty_body_msg(f);

  auto iohp = std::make_shared<io_handler_mock>();
  ip::udp::endpoint endp { };

  test_counter cnt(0);
  msg_hdlr<io_handler_mock> mh(false, cnt);

  int m = 0;
  for (auto i : msgs) {
    auto ret = mh(const_buffer(i.data(), i.size()), io_interface_mock(iohp), endp);
    if (++m % 1000 == 0) {
      REQUIRE(ret);
    }
  }
  REQUIRE_FALSE(mh(const_buffer(empty.data(), empty.size()), io_interface_mock(iohp), endp));

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

SCENARIO ( "Shared Net IP test utility, make msg",
           "[shared_utility] [make_msg]" ) {

  make_msg_test();

}

SCENARIO ( "Shared Net IP test utility, make msg vec",
           "[shared_utility] [make_msg_vec]" ) {
  using namespace chops::test;

  make_msg_vec_test(make_variable_len_msg);
  make_msg_vec_test(make_cr_lf_text_msg);
  make_msg_vec_test(make_lf_text_msg);

}

SCENARIO ( "Shared Net IP test utility, decode variable len msg header",
           "[shared_utility] [decode_variable_len_msg]" ) {
  using namespace chops::test;
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x02, 0x01); // 513 in big endian

  GIVEN ("A two byte buffer that is a variable len msg header") {
    WHEN ("the decode variable len msg hdr function is called") {
      THEN ("the correct length is returned") {
        REQUIRE(decode_variable_len_msg_hdr(ba.data(), 2) == 513);
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

  auto iohp = std::make_shared<io_handler_mock>();
  REQUIRE_FALSE(iohp->send_called);
  ip::udp::endpoint endp { };

  auto msg = make_variable_len_msg(make_body_buf("Bah, humbug!", 'T', 4));
  auto empty = make_empty_variable_len_msg();

  GIVEN ("A mock io handler, a msg with a body, and an empty body msg") {

    WHEN ("a msg hdlr is created with reply true and send is called") {
      test_counter cnt(0);
      msg_hdlr<io_handler_mock> mh(true, cnt);
      THEN ("the shutdown message is handled correctly and count is correct") {
        REQUIRE(mh(const_buffer(msg.data(), msg.size()), io_interface_mock(iohp), endp));
        REQUIRE(iohp->send_called);
        REQUIRE_FALSE(mh(const_buffer(empty.data(), empty.size()), 
                      io_interface_mock(iohp), endp));
        REQUIRE(cnt == 1);
      }
    }
    AND_WHEN ("a msg hdlr is created with reply false and send is called") {
      test_counter cnt(0);
      msg_hdlr<io_handler_mock> mh(false, cnt);
      THEN ("the shutdown message is handled correctly and count is correct") {
        REQUIRE(mh(const_buffer(msg.data(), msg.size()), io_interface_mock(iohp), endp));
        REQUIRE_FALSE(mh(const_buffer(empty.data(), empty.size()), 
                      io_interface_mock(iohp), endp));
        REQUIRE(cnt == 1);
      }
    }
  } // end given
}

SCENARIO ( "Shared Net IP test utility, msg hdlr async stress test",
           "[shared_utility] [msg_hdlr] [stress]" ) {
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

SCENARIO ( "Shared Net IP test utility, io_handler_mock test",
           "[shared_utility] [io_handler_mock]" ) {
  using namespace chops::test;
  using namespace std::experimental::net;

  io_handler_mock io_mock { };

  REQUIRE_FALSE (io_mock.mf_sio_called);
  REQUIRE_FALSE (io_mock.delim_sio_called);
  REQUIRE_FALSE (io_mock.rd_sio_called);
  REQUIRE_FALSE (io_mock.rd_endp_sio_called);
  REQUIRE_FALSE (io_mock.send_sio_called);
  REQUIRE_FALSE (io_mock.send_endp_sio_called);

  auto t = io_mock.sock;

  GIVEN ("A default constructed io_handler_mock") {
    WHEN ("is_io_started is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (io_mock.is_io_started());
      }
    }
    AND_WHEN ("get_socket is called") {
      THEN ("the correct value is returned") {
        REQUIRE (io_mock.get_socket() == t);
      }
    }
    AND_WHEN ("get_socket is used as a modifying call") {
      THEN ("the correct value is returned") {
        io_mock.get_socket() += 2;
        REQUIRE (io_mock.get_socket() == (t+2));
      }
    }
    AND_WHEN ("get_output_queue_stats is called") {
      THEN ("the correct value is returned") {
        auto qs = io_mock.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == io_mock.qs_base);
        REQUIRE (qs.bytes_in_output_queue == (io_mock.qs_base+1));
      }
    }
    AND_WHEN ("first start_io overload is called") {
      THEN ("is_io_started and related flag is true") {
        io_mock.start_io(0, [] { }, [] { });
        REQUIRE (io_mock.is_io_started());
        REQUIRE (io_mock.mf_sio_called);
      }
    }
    AND_WHEN ("second start_io overload is called") {
      THEN ("the related flag is true") {
        io_mock.start_io(std::string_view(), [] { });
        REQUIRE (io_mock.delim_sio_called);
      }
    }
    AND_WHEN ("third start_io overload is called") {
      THEN ("the related flag is true") {
        io_mock.start_io(0, [] { });
        REQUIRE (io_mock.rd_sio_called);
      }
    }
    AND_WHEN ("fourth start_io overload is called") {
      THEN ("the related flag is true") {
        io_mock.start_io(ip::udp::endpoint(), 0, [] { });
        REQUIRE (io_mock.rd_endp_sio_called);
      }
    }
    AND_WHEN ("fifth start_io overload is called") {
      THEN ("the related flag is true") {
        io_mock.start_io();
        REQUIRE (io_mock.send_sio_called);
      }
    }
    AND_WHEN ("sixth start_io overload is called") {
      THEN ("the related flag is true") {
        io_mock.start_io(ip::udp::endpoint());
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

