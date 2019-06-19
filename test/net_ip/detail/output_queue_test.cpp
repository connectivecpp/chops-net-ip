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
#include <thread>
#include <future>
#include <functional> // std::ref, std::cref
#include <numeric> // std::accumulate

#include "net_ip/detail/output_queue.hpp"

#include "marshall/shared_buffer.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

constexpr int WaitTimeBase = 20;
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
std::size_t add_to_q(const std::vector<E>& data_vec, chops::net::detail::output_queue<E>& outq,
                     int multiplier, int wait_offset) {
  chops::repeat(multiplier, [&data_vec, &outq, wait_offset] {
    for (const auto& i : data_vec) {
      outq.add_element(i);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(WaitTimeBase+wait_offset));
  } );
  return data_vec.size() * multiplier;
}

template <typename E>
void output_queue_test(const std::vector<E>& data_vec, int num_thrs, int multiplier) {

  chops::net::detail::output_queue<E> outq { };

  std::vector<std::future<std::size_t>> fut_vec;
  chops::repeat(num_thrs, [&data_vec, &fut_vec, &outq, multiplier] (int i) {
        fut_vec.push_back(std::async(std::launch::async, add_to_q<E>, 
                                     std::cref(data_vec), std::ref(outq), multiplier, 2*i));
  } );
  std::size_t tot = 0u;
  for (auto& fut : fut_vec) {
    tot += fut.get();
  }

  REQUIRE (tot == data_vec.size() * num_thrs * multiplier);
  auto qs = outq.get_queue_stats();
  REQUIRE (qs.output_queue_size == tot);
  REQUIRE (qs.bytes_in_output_queue == accum_buf_size(data_vec) * num_thrs * multiplier);

/*
  auto e = outq.get_next_element();
  REQUIRE (e);
  REQUIRE_FALSE (e->second);
  REQUIRE (e->first == buf);
  chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
  chops::repeat(num_bufs, [&outq] () { outq.get_next_element(); } );
  auto e = outq.get_next_element();
  REQUIRE_FALSE (e);
*/
}

TEST_CASE ( "Output_queue test, single element, 1 thread, multiplier 1", 
           "[output_queue] [single_element] [thread_1] [multiplier_1]" ) {

  output_queue_test(make_buf_vec(), 1, 1);

}

TEST_CASE ( "Output_queue test, single element, 2 threads, multiplier 10", 
           "[output_queue] [single_element] [thread_2] [multiplier_10]" ) {

  output_queue_test(make_buf_vec(), 2, 10);

}

TEST_CASE ( "Output_queue test, single element, 8 threads, multiplier 20", 
           "[output_queue] [single_element] [thread_8] [multiplier_20]" ) {

  output_queue_test(make_buf_vec(), 8, 20);

}

TEST_CASE ( "Output_queue test, double element, 1 thread, multiplier 1",
           "[output_queue] [double_element] [thread_1] [multiplier_1]" ) {

  output_queue_test(make_buf_and_int_vec(), 1, 1);

}

TEST_CASE ( "Output_queue test, double element, 2 threads, multiplier 10",
           "[output_queue] [double_element] [thread_2] [multiplier_10]" ) {

  output_queue_test(make_buf_and_int_vec(), 2, 10);

}

TEST_CASE ( "Output_queue test, double element, 8 threads, multiplier 20",
           "[output_queue] [double_element] [thread_8] [multiplier_20]" ) {

  output_queue_test(make_buf_and_int_vec(), 8, 20);

}

