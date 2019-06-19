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

#include <utility> // std::move
#include <vector>
#include <thread>
#include <future>
#include <functional> // std::ref, std::cref

#include "net_ip/detail/output_queue.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

constexpr int Multiplier = 10;
constexpr int WaitMS = 20;

template <typename E>
std::size_t add_to_q(const std::vector<E>& data_vec, chops::net::detail::output_queue<E>& outq) {
  chops::repeat(Multiplier, [&data_vec, &outq] {
    for (const auto& i : data_vec) {
      outq.add_element(i);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(WaitMS));
  } );
  return data_vec.size() * Multiplier;
}

template <typename E>
void output_queue_test(const std::vector<E>& data_vec, int num_thrs, std::size_t buf_size) {

  chops::net::detail::output_queue<E> outq { };

  std::vector<std::future> fut_vec;
  chops::repeat(num_thrs, [&data_vec, &fut_vec, &outq] () {
        fut_vec.push_back(std::async(std::launch::async, add_to_q<E>, std::cref(data_vec), std::ref(outq)));
  } );
  std::size_t tot = 0u;
  for (auto& fut : fut_vec) {
    tot += fut.get();
  }

  REQUIRE (tot == data_vec.size() * num_thrs * Multiplier);
  auto qs = outq.get_queue_stats();
  REQUIRE (qs.output_queue_size == tot);
  REQUIRE (qs.bytes_in_output_queue == tot * buf_size);

  auto e = outq.get_next_element();
  REQUIRE (e);
  REQUIRE_FALSE (e->second);
  REQUIRE (e->first == buf);
  chops::repeat(num_bufs, [&outq, &buf] () { outq.add_element(buf); } );
  chops::repeat(num_bufs, [&outq] () { outq.get_next_element(); } );
  auto e = outq.get_next_element();
  REQUIRE_FALSE (e);
}

TEST_CASE ( "Output_queue test, single element", 
           "[output_queue]" ) {

  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  add_element_test<asio::ip::udp::endpoint>(chops::const_shared_buffer(std::move(mb)), 10);
  get_next_element_test<asio::ip::udp::endpoint>(chops::const_shared_buffer(std::move(mb)), 20,
                        asio::ip::udp::endpoint(asio::ip::udp::v4(), 1234));
}

TEST_CASE ( "Output_queue test, double element",
           "[output_queue]" ) {

  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  chops::mutable_shared_buffer mb(ba.data(), ba.size());
  add_element_test<asio::ip::tcp::endpoint>(chops::const_shared_buffer(std::move(mb)), 30);
  get_next_element_test<asio::ip::tcp::endpoint>(chops::const_shared_buffer(std::move(mb)), 40,
                        asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 9876));
}

