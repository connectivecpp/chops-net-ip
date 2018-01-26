/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c io_base detail class.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet> // endpoint declarations
#include <experimental/io_context>

#include <memory> // std::shared_ptr
#include <system_error> // std::error_code
#include <utility> // std::move
#include <functional> // std::bind

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_base.hpp"
#include "net_ip/net_ip_error.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

template <typename IOH>
void io_base_test(chops::const_shared_buffer buf, int num_bufs,
                    typename IOH::endpoint_type endp) {

  using namespace std::experimental::net;
  using namespace std::placeholders;
  using notifier_type = typename chops::net::detail::io_base<IOH>::entity_notifier_cb;

  REQUIRE (num_bufs > 1);

  IOH ioh;

  GIVEN ("An io_base and a buf and endpoint") {

    chops::net::detail::io_base<IOH> iobase ( notifier_type(std::bind(&IOH::notify_me, &ioh, _1, _2)) );

    auto qs = iobase.get_output_queue_stats();
    REQUIRE (qs.output_queue_size == 0);
    REQUIRE (qs.bytes_in_output_queue == 0);
    REQUIRE_FALSE (iobase.is_started());
    REQUIRE_FALSE (iobase.is_write_in_progress());

    WHEN ("Process_err_code is called") {
      REQUIRE_FALSE (ioh.notify_called);
      iobase.process_err_code(std::make_error_code(chops::net::net_ip_errc::message_handler_terminated), 
                              std::shared_ptr<IOH>());
      THEN ("the notify_called flag is now true") {
        REQUIRE (ioh.notify_called);
      }
    }

    AND_WHEN ("Set_started is called") {
      bool ret = iobase.set_started();
      REQUIRE (ret);
      THEN ("the started flag is true and write_in_progress flag false") {
        REQUIRE (iobase.is_started());
        REQUIRE_FALSE (iobase.is_write_in_progress());
      }
    }

    AND_WHEN ("Set_started is called twice") {
      bool ret = iobase.set_started();
      REQUIRE (ret);
      ret = iobase.set_started();
      THEN ("the second call returns false") {
        REQUIRE_FALSE (ret);
      }
    }

    AND_WHEN ("Start_write_setup is called before set_started") {
      bool ret = iobase.start_write_setup(buf);
      THEN ("the call returns false") {
        REQUIRE_FALSE (ret);
      }
    }

    AND_WHEN ("Start_write_setup is called after set_started") {
      bool ret = iobase.set_started();
      ret = iobase.start_write_setup(buf);
      THEN ("the call returns true and write_in_progress flag is true and queue size is zero") {
        REQUIRE (ret);
        REQUIRE (iobase.is_write_in_progress());
        REQUIRE (iobase.get_output_queue_stats().output_queue_size == 0);
      }
    }

    AND_WHEN ("Start_write_setup is called twice") {
      bool ret = iobase.set_started();
      ret = iobase.start_write_setup(buf);
      ret = iobase.start_write_setup(buf);
      THEN ("the call returns false and write_in_progress flag is true and queue size is one") {
        REQUIRE_FALSE (ret);
        REQUIRE (iobase.is_write_in_progress());
        REQUIRE (iobase.get_output_queue_stats().output_queue_size == 1);
      }
    }

    AND_WHEN ("Start_write_setup is called many times") {
      bool ret = iobase.set_started();
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iobase, &buf, &ret, &endp] () { 
          ret = iobase.start_write_setup(buf, endp);
        }
      );
      THEN ("all bufs but the first one are queued") {
        REQUIRE (iobase.is_write_in_progress());
        REQUIRE (iobase.get_output_queue_stats().output_queue_size == (num_bufs-1));
      }
    }

    AND_WHEN ("Start_write_setup is called many times and get_next_element is called 2 less times") {
      bool ret = iobase.set_started();
      REQUIRE (ret);
      chops::repeat(num_bufs, [&iobase, &buf, &ret, &endp] () { 
          iobase.start_write_setup(buf, endp);
        }
      );
      chops::repeat((num_bufs - 2), [&iobase] () { 
          iobase.get_next_element();
        }
      );
      THEN ("next get_next_element call returns a val, call after doesn't and write_in_progress is false") {

        auto qs = iobase.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 1);
        REQUIRE (qs.bytes_in_output_queue == buf.size());

        auto e = iobase.get_next_element();

        qs = iobase.get_output_queue_stats();
        REQUIRE (qs.output_queue_size == 0);
        REQUIRE (qs.bytes_in_output_queue == 0);

        REQUIRE (iobase.is_write_in_progress());

        REQUIRE (e);
        REQUIRE (e->first == buf);
        REQUIRE (e->second == endp);

        auto e2 = iobase.get_next_element();
        REQUIRE_FALSE (iobase.is_write_in_progress());
        REQUIRE_FALSE (e2);

      }
    }

  } // end given
}

struct io_mock {
  bool notify_called = false;

  using endpoint_type = float;

  void notify_me(std::error_code err, std::shared_ptr<io_mock> p) {
    notify_called = true;
  }

};

SCENARIO ( "Io base test, udp", "[io_base]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_base_test<io_mock>(chops::const_shared_buffer(std::move(mb)), 20, 42.0);
}

