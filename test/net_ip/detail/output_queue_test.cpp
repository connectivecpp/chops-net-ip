/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c output_queue detail class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <vector>
#include <functional> // std::ref, std::cref
#include <numeric> // std::accumulate
#include <cassert>

#include "net_ip/detail/output_queue.hpp"

#include "marshall/shared_buffer.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

constexpr int Answer = 42;

struct buf_and_int {
  chops::const_shared_buffer  m_buf;
  int                         m_num;

  buf_and_int (const chops::const_shared_buffer& buf) : m_buf(buf), m_num(Answer) { }

  std::size_t size() const noexcept { return m_buf.size(); }

};

auto make_buf1() {
  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

auto make_buf2() {
  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

auto make_buf_vec() {
  return std::vector<chops::const_shared_buffer> { make_buf1(), make_buf2() };
}

auto make_buf_and_int_vec() {
  return std::vector<buf_and_int> { buf_and_int(make_buf1()), buf_and_int(make_buf2()) };
}

template <typename E>
std::size_t accum_buf_size (const std::vector<E>& data_vec) {
  return std::accumulate (data_vec.cbegin(), data_vec.cend(), static_cast<std::size_t>(0u),
    [] (std::size_t sum, const E& elem) { return sum += elem.size(); } );
}

template <typename E>
std::size_t add_to_q(const std::vector<E>& data_vec, 
                     chops::net::detail::output_queue<E>& outq,
                     int multiplier) {
  chops::repeat(multiplier, [&data_vec, &outq] {
      for (const auto& i : data_vec) {
        outq.add_element(i);
      }
  } );
  return data_vec.size() * multiplier;
}

template <typename E>
void output_queue_test(const std::vector<E>& data_vec, int multiplier) {

  chops::net::detail::output_queue<E> outq { };

  auto tot = add_to_q(data_vec, outq, multiplier);

  REQUIRE (tot == data_vec.size() * multiplier);
  auto qs = outq.get_queue_stats();
  REQUIRE (qs.output_queue_size == tot);
  REQUIRE (qs.bytes_in_output_queue == accum_buf_size(data_vec) * multiplier);

  chops::repeat(tot, [&outq] {
      auto e = outq.get_next_element();
//      REQUIRE (e);
      assert (e);
    }
  );
  auto e = outq.get_next_element(); // should be empty optional
  REQUIRE_FALSE (e); // no element val available
  qs = outq.get_queue_stats();
  REQUIRE (qs.output_queue_size == 0u);
  REQUIRE (qs.bytes_in_output_queue == 0u);

  auto t = add_to_q(data_vec, outq, 1);
  qs = outq.get_queue_stats();
  REQUIRE_FALSE (qs.output_queue_size == 0u);

  outq.clear();
  qs = outq.get_queue_stats();
  REQUIRE (qs.output_queue_size == 0u);
  REQUIRE (qs.bytes_in_output_queue == 0u);
}

TEST_CASE ( "Output_queue test, single element, multiplier 1", 
           "[output_queue] [single_element] [multiplier_1]" ) {

  output_queue_test(make_buf_vec(), 1);

}

TEST_CASE ( "Output_queue test, single element, multiplier 10", 
           "[output_queue] [single_element] [multiplier_10]" ) {

  output_queue_test(make_buf_vec(), 10);

}

TEST_CASE ( "Output_queue test, single element, multiplier 20", 
           "[output_queue] [single_element] [multiplier_20]" ) {

  output_queue_test(make_buf_vec(), 20);

}

TEST_CASE ( "Output_queue test, single element, multiplier 50", 
           "[output_queue] [single_element] [multiplier_50]" ) {

  output_queue_test(make_buf_vec(), 50);

}

TEST_CASE ( "Output_queue test, double element, multiplier 1",
           "[output_queue] [double_element] [multiplier_1]" ) {

  output_queue_test(make_buf_and_int_vec(), 1);

}

TEST_CASE ( "Output_queue test, double element, multiplier 10",
           "[output_queue] [double_element] [multiplier_10]" ) {

  output_queue_test(make_buf_and_int_vec(), 10);

}

TEST_CASE ( "Output_queue test, double element, multiplier 20",
           "[output_queue] [double_element] [multiplier_20]" ) {

  output_queue_test(make_buf_and_int_vec(), 20);

}

TEST_CASE ( "Output_queue test, double element, multiplier 50",
           "[output_queue] [double_element] [multiplier_50]" ) {

  output_queue_test(make_buf_and_int_vec(), 50);

}

