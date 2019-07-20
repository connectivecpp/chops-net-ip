/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c io_common detail class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <vector>
#include <cassert>

#include <future>
#include <thread>
#include <chrono>
#include <functional> // std::cref, std::ref
#include <cstddef> // std::size_t

#include "net_ip/detail/io_common.hpp"

#include "marshall/shared_buffer.hpp"

#include "utility/repeat.hpp"

#include "shared_test/io_buf.hpp"

template <typename E>
void empty_write_func (const E&) { }

template <typename E>
void check_queue_stats(const chops::net::detail::io_common<E>& ioc,
                       std::size_t exp_qs, std::size_t exp_bs) {

  auto qs = ioc.get_output_queue_stats();
  REQUIRE (qs.output_queue_size == exp_qs);
  REQUIRE (qs.bytes_in_output_queue == exp_bs);

}

template <typename E>
void io_common_api_test(const E& elem) {

  chops::net::detail::io_common<E> iocommon { };

  check_queue_stats(iocommon, 0u, 0u);

  REQUIRE_FALSE (iocommon.is_io_started());
  REQUIRE_FALSE (iocommon.is_write_in_progress());

  auto s = iocommon.start_write(elem, empty_write_func<E>);
  REQUIRE (s == chops::net::detail::io_common<E>::write_status::io_stopped);
  REQUIRE_FALSE (iocommon.set_io_stopped());

  // start io
  REQUIRE (iocommon.set_io_started());
  REQUIRE (iocommon.is_io_started());
  // write elems, queueing starts with second write
  s = iocommon.start_write(elem, empty_write_func<E>);
  REQUIRE (s == chops::net::detail::io_common<E>::write_status::write_started);
  check_queue_stats(iocommon, 0u, 0u);
  REQUIRE (iocommon.is_write_in_progress());
  s = iocommon.start_write(elem, empty_write_func<E>);
  REQUIRE (s == chops::net::detail::io_common<E>::write_status::queued);
  check_queue_stats(iocommon, 1u, 1u*elem.size());
  REQUIRE (iocommon.is_write_in_progress());
  s = iocommon.start_write(elem, empty_write_func<E>);
  REQUIRE (s == chops::net::detail::io_common<E>::write_status::queued);
  check_queue_stats(iocommon, 2u, 2u*elem.size());
  REQUIRE (iocommon.is_write_in_progress());

  // start dequeing
  iocommon.write_next_elem(empty_write_func<E>);
  check_queue_stats(iocommon, 1u, 1u*elem.size());
  REQUIRE (iocommon.is_write_in_progress());
  iocommon.write_next_elem(empty_write_func<E>);
  check_queue_stats(iocommon, 0u, 0u);
  REQUIRE (iocommon.is_write_in_progress());
  iocommon.write_next_elem(empty_write_func<E>);
  check_queue_stats(iocommon, 0u, 0u);
  REQUIRE_FALSE (iocommon.is_write_in_progress());

  REQUIRE_FALSE (iocommon.set_io_started());
  REQUIRE (iocommon.set_io_stopped());

  // test clear method
  iocommon.set_io_started();
  iocommon.start_write(elem, empty_write_func<E>);
  iocommon.start_write(elem, empty_write_func<E>);
  check_queue_stats(iocommon, 1u, 1u*elem.size());
  iocommon.clear();
  check_queue_stats(iocommon, 0u, 0u);
  REQUIRE_FALSE (iocommon.is_write_in_progress());

}

constexpr int Wait = 5;

template <typename E>
std::size_t start_writes(const std::vector<E>& data_vec, 
                         chops::net::detail::io_common<E>& io_comm,
                         int multiplier, int wait_offset) {
  chops::repeat(multiplier, [&data_vec, &io_comm, wait_offset] {
      for (const auto& e : data_vec) {
        auto r = io_comm.start_write(e, empty_write_func<E>);
        assert (r != chops::net::detail::io_common<E>::write_status::io_stopped);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(Wait+wait_offset));
      assert (io_comm.is_io_started());
  } );
  return data_vec.size() * multiplier;
}

template <typename E>
std::size_t write_next_elems(const std::vector<E>& data_vec, 
                             chops::net::detail::io_common<E>& io_comm) {
  while (io_comm.is_write_in_progress()) {
    io_comm.write_next_elem(empty_write_func<E>);
    assert (io_comm.is_io_started());
  }
  return 0u;
}

template <typename E>
void io_common_stress_test(const std::vector<E>& data_vec, int multiplier, int num_thrs) {

  chops::net::detail::io_common<E> iocommon { };
  REQUIRE(iocommon.set_io_started());

  std::vector<std::future<std::size_t>> futs;

  chops::repeat(num_thrs, [&iocommon, multiplier, &data_vec, &futs] (int i) {
    futs.push_back(std::async(std::launch::async, start_writes<E>,
                              std::cref(data_vec), std::ref(iocommon),
                              multiplier, 2*i));
  } );

  std::size_t tot = 0u;
  for (auto& fut : futs) {
    tot += fut.get(); // join threads, get all values
  }

  REQUIRE (tot == data_vec.size() * multiplier * num_thrs);
  check_queue_stats(iocommon, tot - 1u, (chops::test::accum_io_buf_size(data_vec) * 
                                        multiplier * num_thrs) - data_vec.front().size());

  futs.clear();
  
  chops::repeat(num_thrs, [&iocommon, &data_vec, &futs] {
    futs.push_back(std::async(std::launch::async, write_next_elems<E>,
                              std::cref(data_vec), std::ref(iocommon)));
  } );

  for (auto& fut : futs) {
    tot += fut.get(); // join threads
  }
  check_queue_stats(iocommon, 0u, 0u);

  REQUIRE(iocommon.set_io_stopped());
}


TEST_CASE ( "Io common API test, single element", 
           "[io_common] [single_element] [api]" ) {

  io_common_api_test(chops::test::make_io_buf1());

}

TEST_CASE ( "Io common API test, double element", 
           "[io_common] [double_element] [api]" ) {

  io_common_api_test(chops::test::io_buf_and_int(chops::test::make_io_buf2()));

}

TEST_CASE ( "Io common stress test, single element, multiplier 1, 1 thread", 
           "[io_common] [single_element] [multiplier_1] [threads_1]" ) {

  io_common_stress_test(chops::test::make_io_buf_vec(), 1, 1);

}

TEST_CASE ( "Io common stress test, single element, multiplier 10, 1 thread", 
           "[io_common] [single_element] [multiplier_10] [threads_1]" ) {

  io_common_stress_test(chops::test::make_io_buf_vec(), 10, 1);

}

TEST_CASE ( "Io common stress test, single element, multiplier 20, 5 threads", 
           "[io_common] [single_element] [multiplier_20] [threads_5]" ) {

  io_common_stress_test(chops::test::make_io_buf_vec(), 20, 5);

}

TEST_CASE ( "Io common stress test, single element, multiplier 50, 10 threads", 
           "[io_common] [single_element] [multiplier_50] [threads_10]" ) {

  io_common_stress_test(chops::test::make_io_buf_vec(), 50, 10);

}

TEST_CASE ( "Io common stress test, single element, multiplier 100, 60 threads", 
           "[io_common] [single_element] [multiplier_100] [threads_60]" ) {

  io_common_stress_test(chops::test::make_io_buf_vec(), 100, 60);

}

TEST_CASE ( "Io common stress test, double element, multiplier 1, 1 thread",
           "[io_common] [double_element] [multiplier_1] [threads_1]" ) {

  io_common_stress_test(chops::test::make_io_buf_and_int_vec(), 1, 1);

}

TEST_CASE ( "Io common stress test, double element, multiplier 10, 1 thread",
           "[io_common] [double_element] [multiplier_10] [threads_1]" ) {

  io_common_stress_test(chops::test::make_io_buf_and_int_vec(), 10, 1);

}

TEST_CASE ( "Io common stress test, double element, multiplier 20, 5 threads",
           "[io_common] [double_element] [multiplier_20] [threads_5]" ) {

  io_common_stress_test(chops::test::make_io_buf_and_int_vec(), 20, 5);

}

TEST_CASE ( "Io common stress test, double element, multiplier 50, 10 threads",
           "[io_common] [double_element] [multiplier_50] [threads_10]" ) {

  io_common_stress_test(chops::test::make_io_buf_and_int_vec(), 50, 10);

}

TEST_CASE ( "Io common stress test, double element, multiplier 100, 60 threads",
           "[io_common] [double_element] [multiplier_100] [threads_60]" ) {

  io_common_stress_test(chops::test::make_io_buf_and_int_vec(), 100, 60);

}

