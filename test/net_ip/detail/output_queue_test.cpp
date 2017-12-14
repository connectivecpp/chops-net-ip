/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c output_queue detail class.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <utility> // std::move

#include "net_ip/detail/output_queue.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

void add_element_test(chops::const_shared_buffer buf, int num_bufs) {

  GIVEN ("A default constructed output_queue") {
    chops::net::detail::output_queue outq { };

    WHEN ("bufs are added to the output_queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      THEN ("the queue_stats match") {
        auto qs = outq.get_queue_stats();
        REQUIRE (qs.output_queue_size == num_bufs);
        REQUIRE (qs.bytes_in_output_queue == (num_bufs * buf.size()));
      }
    }
    WHEN ("A value is removed from the queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      auto e = outq.get_next_element();
      THEN ("it is not empty and values match") {
        REQUIRE (e);
        REQUIRE (!e->second);
        REQUIRE (e->first == buf);
      }
    }
    WHEN ("All values are removed from the queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      chops::repeat(num_bufs, [&outq] () { outq.get_next_element(); } );
      THEN ("an empty element will be returned next") {
        auto e = outq.get_next_element();
        REQUIRE (!e);
      }
    }
  } // end given
}

SCENARIO ( "Output_queue test, buf only", "[output_queue_buf_only]" ) {
  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  add_element_test(chops::const_shared_buffer(std::move(mb)), 10);
}

