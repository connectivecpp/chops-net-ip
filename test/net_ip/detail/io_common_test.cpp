/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c io_common detail class.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet> // endpoint declarations
#include <experimental/socket>
#include <experimental/io_context>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_common.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

template <typename Protocol, typename Sock>
bool call_start_io_setup(chops::net::detail::io_common<Protocol>& comm) {

  using namespace std::experimental::net;

  io_context ioc;
  Sock sock(ioc);
  return comm.start_io_setup(sock);
}

template <typename Protocol, typename Sock>
void io_common_test(chops::const_shared_buffer buf, int num_bufs,
                    const std::experimental::net::ip::basic_endpoint<Protocol>& endp) {

  using namespace std::experimental::net;

  REQUIRE (num_bufs > 1);

  GIVEN ("A default constructed io_common and a buf and endpoint") {

    INFO ("Endpoint passed as arg: " << endp);
    chops::net::detail::io_common<Protocol> iocommon { };
    INFO("Remote endpoint after default construction: " << iocommon.get_remote_endp());

    auto qs = iocommon.get_output_queue_stats();
    REQUIRE (qs.output_queue_size == 0);
    REQUIRE (qs.bytes_in_output_queue == 0);
    REQUIRE (!iocommon.is_started());
    REQUIRE (!iocommon.is_write_in_progress());

    WHEN ("Start_io_setup is called") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      REQUIRE (ret);
      THEN ("the started flag is true and write_in_progress flag false") {
        REQUIRE (iocommon.is_started());
        REQUIRE (!iocommon.is_write_in_progress());
      }
    }

    AND_WHEN ("Start_io_setup is called twice") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      REQUIRE (ret);
      ret = call_start_io_setup<Protocol, Sock>(iocommon);
      THEN ("the second call returns false") {
        REQUIRE (!ret);
      }
    }

    AND_WHEN ("Start_write_setup is called before start_io_setup") {
      bool ret = iocommon.start_write_setup(buf);
      THEN ("the call returns false") {
        REQUIRE (!ret);
      }
    }

    AND_WHEN ("Start_write_setup is called after start_io_setup") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      ret = iocommon.start_write_setup(buf);
      THEN ("the call returns true and write_in_progress flag is true and queue size is zero") {
        REQUIRE (ret);
        REQUIRE (iocommon.is_write_in_progress());
        REQUIRE (iocommon.get_output_queue_stats().output_queue_size == 0);
      }
    }

    AND_WHEN ("Start_write_setup is called twice") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      ret = iocommon.start_write_setup(buf);
      ret = iocommon.start_write_setup(buf);
      THEN ("the call returns false and write_in_progress flag is true and queue size is one") {
        REQUIRE (!ret);
        REQUIRE (iocommon.is_write_in_progress());
        REQUIRE (iocommon.get_output_queue_stats().output_queue_size == 1);
      }
    }

    AND_WHEN ("Start_write_setup is called many times") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iocommon, &buf, &ret, &endp] () { 
          ret = iocommon.start_write_setup(buf, endp);
        }
      );
      THEN ("all bufs but the first one are queued") {
        REQUIRE (iocommon.is_write_in_progress());
        REQUIRE (iocommon.get_output_queue_stats().output_queue_size == (num_bufs-1));
      }
    }

    AND_WHEN ("Start_write_setup is called many times and get_next_element is called 2 less times") {
      bool ret = call_start_io_setup<Protocol, Sock>(iocommon);
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iocommon, &buf, &ret, &endp] () { 
          iocommon.start_write_setup(buf, endp);
        }
      );
      chops::repeat((num_bufs - 2), [&iocommon] () { 
          iocommon.get_next_element();
        }
      );
      THEN ("next get_next_element call returns a val, call after doesn't and write_in_progress is false") {

        auto qs = iocommon.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 1);
        REQUIRE (qs.bytes_in_output_queue == buf.size());

        auto e = iocommon.get_next_element();

        qs = iocommon.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 0);
        REQUIRE (qs.bytes_in_output_queue == 0);

        REQUIRE (iocommon.is_write_in_progress());

        REQUIRE (e);
        REQUIRE (e->first == buf);
        REQUIRE (e->second == endp);

        auto e2 = iocommon.get_next_element();
        REQUIRE (!iocommon.is_write_in_progress());
        REQUIRE (!e2);

      }
    }

  } // end given
}

SCENARIO ( "Io common test, udp endpoint", "[io_common_udp_endpoint]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_common_test<ip::udp, ip::udp::socket>(chops::const_shared_buffer(std::move(mb)), 20,
                          ip::udp::endpoint(ip::udp::v4(), 1234));
}

SCENARIO ( "Io common test, tcp endpoint", "[io_common_tcp_endpoint]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_common_test<ip::tcp, ip::tcp::socket>(chops::const_shared_buffer(std::move(mb)), 40,
                          ip::tcp::endpoint(ip::tcp::v6(), 9876));
}
