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
#include <experimental/io_context>

#include <memory> // std::shared_ptr
#include <system_error> // std::error_code
#include <utility> // std::move

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_common.hpp"
#include "net_ip/net_ip_error.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

template <typename IOT>
void io_common_test(chops::const_shared_buffer buf, int num_bufs,
                    typename IOT::endpoint_type endp) {

  using namespace std::experimental::net;
  using namespace std::placeholders;

  REQUIRE (num_bufs > 1);

  IOT ioh;

  GIVEN ("An io_common and a buf and endpoint") {

    chops::net::detail::io_common<IOT> iocommon { };

    auto qs = iocommon.get_output_queue_stats();
    REQUIRE (qs.output_queue_size == 0);
    REQUIRE (qs.bytes_in_output_queue == 0);
    REQUIRE_FALSE (iocommon.is_io_started());
    REQUIRE_FALSE (iocommon.is_write_in_progress());

    WHEN ("Set_io_started is called") {
      bool ret = iocommon.set_io_started();
      REQUIRE (ret);
      THEN ("the io_started flag is true and write_in_progress flag false") {
        REQUIRE (iocommon.is_io_started());
        REQUIRE_FALSE (iocommon.is_write_in_progress());
      }
    }

    AND_WHEN ("Set_io_started is called twice") {
      bool ret = iocommon.set_io_started();
      REQUIRE (ret);
      ret = iocommon.set_io_started();
      THEN ("the second call returns false") {
        REQUIRE_FALSE (ret);
      }
    }

    AND_WHEN ("Start_write_setup is called before set_io_started") {
      bool ret = iocommon.start_write_setup(buf);
      THEN ("the call returns false") {
        REQUIRE_FALSE (ret);
      }
    }

    AND_WHEN ("Start_write_setup is called after set_io_started") {
      bool ret = iocommon.set_io_started();
      ret = iocommon.start_write_setup(buf);
      THEN ("the call returns true and write_in_progress flag is true and queue size is zero") {
        REQUIRE (ret);
        REQUIRE (iocommon.is_write_in_progress());
        REQUIRE (iocommon.get_output_queue_stats().output_queue_size == 0);
      }
    }

    AND_WHEN ("Start_write_setup is called twice") {
      bool ret = iocommon.set_io_started();
      ret = iocommon.start_write_setup(buf);
      ret = iocommon.start_write_setup(buf);
      THEN ("the call returns false and write_in_progress flag is true and queue size is one") {
        REQUIRE_FALSE (ret);
        REQUIRE (iocommon.is_write_in_progress());
        REQUIRE (iocommon.get_output_queue_stats().output_queue_size == 1);
      }
    }

    AND_WHEN ("Start_write_setup is called many times") {
      bool ret = iocommon.set_io_started();
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
      bool ret = iocommon.set_io_started();
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
        REQUIRE_FALSE (iocommon.is_write_in_progress());
        REQUIRE_FALSE (e2);

      }
    }

  } // end given
}

struct io_mock {
  using endpoint_type = float;

};

SCENARIO ( "Io common test", "[io_common]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  io_common_test<io_mock>(chops::const_shared_buffer(std::move(mb)), 20, 42.0);
}

