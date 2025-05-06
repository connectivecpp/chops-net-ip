/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Shared code used in IO unit tests, such as @c output_queue_test.cpp and
 *  @c io_common_test.cpp.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef IO_BUF_HPP_INCLUDED
#define IO_BUF_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <numeric> // std::accumulate
#include <vector>

#include "buffer/shared_buffer.hpp"
#include "utility/byte_array.hpp"


namespace chops {
namespace test {

constexpr int Answer = 42;

struct io_buf_and_int {
  chops::const_shared_buffer  m_buf;
  int                         m_num;

  io_buf_and_int (const chops::const_shared_buffer& buf) : m_buf(buf), m_num(Answer) { }

  std::size_t size() const noexcept { return m_buf.size(); }

};

auto make_io_buf1() {
  auto ba = chops::make_byte_array(0x20, 0x21, 0x22, 0x23, 0x24);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

auto make_io_buf2() {
  auto ba = chops::make_byte_array(0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

auto make_io_buf_vec() {
  return std::vector<chops::const_shared_buffer> { make_io_buf1(), make_io_buf2() };
}

auto make_io_buf_and_int_vec() {
  return std::vector<io_buf_and_int> { io_buf_and_int(make_io_buf1()), io_buf_and_int(make_io_buf2()) };
}

template <typename E>
std::size_t accum_io_buf_size (const std::vector<E>& data_vec) {
  return std::accumulate (data_vec.cbegin(), data_vec.cend(), static_cast<std::size_t>(0u),
    [] (std::size_t sum, const E& elem) { return sum += elem.size(); } );
}


} // end namespace test
} // end namespace chops

#endif

