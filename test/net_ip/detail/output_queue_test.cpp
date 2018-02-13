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

#include "catch.hpp"

#include <utility> // std::move

#include <experimental/internet> // endpoint declarations

#include "net_ip/detail/output_queue.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

template <typename E>
void add_element_test(chops::const_shared_buffer buf, int num_bufs) {

  GIVEN ("A default constructed output_queue") {
    chops::net::detail::output_queue<E> outq { };

    WHEN ("Bufs are added to the output_queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      THEN ("the queue_stats match") {
        auto qs = outq.get_queue_stats();
        REQUIRE (qs.output_queue_size == num_bufs);
        REQUIRE (qs.bytes_in_output_queue == (num_bufs * buf.size()));
      }
    }
    AND_WHEN ("A value is removed from the queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      auto e = outq.get_next_element();
      THEN ("it is not empty and values match") {
        REQUIRE (e);
        REQUIRE_FALSE (e->second);
        REQUIRE (e->first == buf);
      }
    }
    AND_WHEN ("All values are removed from the queue") {
      chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
      chops::repeat(num_bufs, [&outq] () { outq.get_next_element(); } );
      THEN ("an empty element will be returned next") {
        auto e = outq.get_next_element();
        REQUIRE_FALSE (e);
      }
    }
  } // end given
}

template <typename E>
void get_next_element_test(chops::const_shared_buffer buf, int num_bufs,
                           const E& endp) {

  GIVEN ("A default constructed output_queue") {
    chops::net::detail::output_queue<E> outq { };

    WHEN ("Bufs and endpoints are added to the output_queue") {
      chops::repeat(num_bufs, [&outq, &buf, &endp] () { outq.add_element(buf, endp); } );
      THEN ("a matching number of bufs and endpoints can be removed") {
        auto qs = outq.get_queue_stats();
        REQUIRE (qs.output_queue_size == num_bufs);
        chops::repeat(num_bufs, [&outq, &buf, &endp] () {
            auto e = outq.get_next_element();
            REQUIRE (e);
            REQUIRE (e->first == buf);
            REQUIRE (e->second);
            REQUIRE (e->second == endp);
          }
        );
      }
    } // end when
  } // end given
}

SCENARIO ( "Output_queue test, udp endpoint", 
           "[output_queue] [udp]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  add_element_test<ip::udp::endpoint>(chops::const_shared_buffer(std::move(mb)), 10);
  get_next_element_test<ip::udp::endpoint>(chops::const_shared_buffer(std::move(mb)), 20,
                        ip::udp::endpoint(ip::udp::v4(), 1234));
}

SCENARIO ( "Output_queue test, tcp endpoint",
           "[output_queue] [tcp]" ) {
  using namespace std::experimental::net;

  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  add_element_test<ip::tcp::endpoint>(chops::const_shared_buffer(std::move(mb)), 30);
  get_next_element_test<ip::tcp::endpoint>(chops::const_shared_buffer(std::move(mb)), 40,
                        ip::tcp::endpoint(ip::tcp::v6(), 9876));
}

