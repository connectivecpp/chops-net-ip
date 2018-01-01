/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
 *
 *  The test strategy is to create mock net entity classes to test the interface
 *  and functionality of the @c tcp_io class. In particular, the notification hooks
 *  between the two levels is tricky and needs to be well tested.
 *
 *  The data flow is tested by: ( fill in details )
 *
 *  
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet> // endpoint declarations
#include <experimental/socket>
#include <experimental/io_context>

#include <system_error> // std::error_code
#include <string_view>
#include <string>
#include <cstddef> // std::size_t

#include "net_ip/detail/tcp_io.hpp"

#include "utility/repeat.hpp"
#include "utility/shared_buffer.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"


void io_common_test(chops::const_shared_buffer buf, int num_bufs,
                    const std::experimental::net::ip::basic_endpoint<typename IOH::protocol_type>& endp) {

  using namespace std::experimental::net;
  using namespace std::placeholders;
  using notifier_type = typename chops::net::detail::io_common<IOH>::entity_notifier_cb;

  REQUIRE (num_bufs > 1);

  IOH ioh;

  GIVEN ("An io_common and a buf and endpoint") {

    INFO ("Endpoint passed as arg: " << endp);
    chops::net::detail::io_common<IOH> iocomm ( notifier_type(std::bind(&IOH::notify_me, &ioh, _1, _2)) );
    INFO("Remote endpoint after default construction: " << iocomm.get_remote_endp());

    auto qs = iocomm.get_output_queue_stats();
    REQUIRE (qs.output_queue_size == 0);
    REQUIRE (qs.bytes_in_output_queue == 0);
    REQUIRE (!iocomm.is_started());
    REQUIRE (!iocomm.is_write_in_progress());

    WHEN ("Check_err_code is called") {
      REQUIRE (!ioh.notify_called);
      bool ret = iocomm.check_err_code(std::make_error_code(chops::net::net_ip_errc::message_handler_terminated), std::shared_ptr<IOH>());
      THEN ("the notify_called flag is now true and the return is false") {
        REQUIRE (ioh.notify_called);
        REQUIRE (!ret);
      }
    }

    AND_WHEN ("Start_io_setup is called") {
      bool ret = call_start_io_setup(iocomm);
      REQUIRE (ret);
      THEN ("the started flag is true and write_in_progress flag false") {
        REQUIRE (iocomm.is_started());
        REQUIRE (!iocomm.is_write_in_progress());
      }
    }

    AND_WHEN ("Start_io_setup is called twice") {
      bool ret = call_start_io_setup(iocomm);
      REQUIRE (ret);
      ret = call_start_io_setup(iocomm);
      THEN ("the second call returns false") {
        REQUIRE (!ret);
      }
    }

    AND_WHEN ("Start_write_setup is called before start_io_setup") {
      bool ret = iocomm.start_write_setup(buf);
      THEN ("the call returns false") {
        REQUIRE (!ret);
      }
    }

    AND_WHEN ("Start_write_setup is called after start_io_setup") {
      bool ret = call_start_io_setup(iocomm);
      ret = iocomm.start_write_setup(buf);
      THEN ("the call returns true and write_in_progress flag is true and queue size is zero") {
        REQUIRE (ret);
        REQUIRE (iocomm.is_write_in_progress());
        REQUIRE (iocomm.get_output_queue_stats().output_queue_size == 0);
      }
    }

    AND_WHEN ("Start_write_setup is called twice") {
      bool ret = call_start_io_setup(iocomm);
      ret = iocomm.start_write_setup(buf);
      ret = iocomm.start_write_setup(buf);
      THEN ("the call returns false and write_in_progress flag is true and queue size is one") {
        REQUIRE (!ret);
        REQUIRE (iocomm.is_write_in_progress());
        REQUIRE (iocomm.get_output_queue_stats().output_queue_size == 1);
      }
    }

    AND_WHEN ("Start_write_setup is called many times") {
      bool ret = call_start_io_setup(iocomm);
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iocomm, &buf, &ret, &endp] () { 
          ret = iocomm.start_write_setup(buf, endp);
        }
      );
      THEN ("all bufs but the first one are queued") {
        REQUIRE (iocomm.is_write_in_progress());
        REQUIRE (iocomm.get_output_queue_stats().output_queue_size == (num_bufs-1));
      }
    }

    AND_WHEN ("Start_write_setup is called many times and get_next_element is called 2 less times") {
      bool ret = call_start_io_setup(iocomm);
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iocomm, &buf, &ret, &endp] () { 
          iocomm.start_write_setup(buf, endp);
        }
      );
      chops::repeat((num_bufs - 2), [&iocomm] () { 
          iocomm.get_next_element();
        }
      );
      THEN ("next get_next_element call returns a val, call after doesn't and write_in_progress is false") {

        auto qs = iocomm.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 1);
        REQUIRE (qs.bytes_in_output_queue == buf.size());

        auto e = iocomm.get_next_element();

        qs = iocomm.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 0);
        REQUIRE (qs.bytes_in_output_queue == 0);

        REQUIRE (iocomm.is_write_in_progress());

        REQUIRE (e);
        REQUIRE (e->first == buf);
        REQUIRE (e->second == endp);

        auto e2 = iocomm.get_next_element();
        REQUIRE (!iocomm.is_write_in_progress());
        REQUIRE (!e2);

      }
    }

  } // end given
}

struct tcp_entity_mock {
  bool notify_called = false;

  using socket_type = std::experimental::net::ip::tcp::socket;
  using protocol_type = std::experimental::net::ip::tcp;

  void notify_me(const std::error_code& err, std::shared_ptr<tcp_io_mock> p) {
    notify_called = true;
  }

};

SCENARIO ( "Io common test, udp", "[io_common_udp]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_common_test<udp_io_mock>(chops::const_shared_buffer(std::move(mb)), 20,
                          ip::udp::endpoint(ip::udp::v4(), 1234));
}

SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_common_test<tcp_io_mock>(chops::const_shared_buffer(std::move(mb)), 40,
                          ip::tcp::endpoint(ip::tcp::v6(), 9876));
}

