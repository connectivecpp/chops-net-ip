/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for Blitz Rakete's utility function.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch.hpp"

#include "utility/make_byte_array.hpp"
#include "utility/repeat.hpp"

SCENARIO( "Blitz Rakete's utility function conveniently creates a std::array of std::bytes", 
           "[make_byte_array]" ) {
  GIVEN ("Various integers to put into a std::array of std::bytes") {
    WHEN ("The function is called with five arguments") {
      auto arr = chops::make_byte_array(0x36, 0xd0, 0x42, 0xbe, 0xef);
      THEN ("the size and contents should match") {
        REQUIRE (arr.size() == 5);
        REQUIRE (arr[0] == std::byte{0x36});
        REQUIRE (arr[1] == std::byte{0xd0});
        REQUIRE (arr[2] == std::byte{0x42});
        REQUIRE (arr[3] == std::byte{0xbe});
        REQUIRE (arr[4] == std::byte{0xef});
      }
    }
    AND_WHEN ("The function is called with eleven arguments") {
      constexpr int N = 11;
      auto arr = chops::make_byte_array(
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
      );
      THEN ("the size and contents should match") {
        REQUIRE (arr.size() == N);
        chops::repeat(N, [&arr] (const int& i) { REQUIRE (arr[i] == std::byte{0x11}); } );
      }
    }
  } // end given
}
