/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test @c io_buf.hpp.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include "shared_test/io_buf.hpp"

TEST_CASE ( "IO buf testing",
           "[io_buf]" ) {

  using namespace chops::test;

  auto b1 = make_io_buf1();
  REQUIRE (static_cast<int>(*(b1.data()+1)) == 0x21);
  auto b2 = make_io_buf2();
  REQUIRE (static_cast<int>(*(b2.data()+2)) == 0x42);
  auto bv1 = make_io_buf_vec();
  REQUIRE (bv1.size() == 2u);
  REQUIRE (static_cast<int>(*(bv1[0].data()+0)) == 0x20);
  REQUIRE (static_cast<int>(*(bv1[1].data()+0)) == 0x40);
  auto bv2 = make_io_buf_and_int_vec();
  REQUIRE (bv2.size() == 2u);
  REQUIRE (bv2[0].m_num == 42);
  REQUIRE (bv2[1].m_num == 42);
  REQUIRE (accum_io_buf_size(bv2) == 12u);
}

