/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c mutable_shared_buffer and
 *  @c const_shared_buffer classes.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#define CATCH_CONFIG_MAIN

#include "catch.hpp"


#include <cstddef> // std::byte
#include <list>

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"

constexpr std::byte Harhar { 42 };

template <typename SB, typename T>
void pointer_check(const T* bp, typename SB::size_type sz) {

  GIVEN ("An arbitrary buf pointer and a size") {
    WHEN ("A shared buffer is constructed with the buf and size") {
      SB sb(bp, sz);
      THEN ("the shared buffer is not empty, the size matches and the contents match") {
        REQUIRE (!sb.empty());
        REQUIRE (sb.size() == sz);
        const std::byte* buf = static_cast<const std::byte*>(static_cast<const void*>(bp));
        chops::repeat(sz, [&sb, buf] (const int& i) { REQUIRE(*(sb.data()+i) == *(buf+i)); } );
      }
    }
  } // end given
}

template <typename SB>
void shared_buffer_common(const std::byte* buf, typename SB::size_type sz) {

  REQUIRE (sz > 2);

  pointer_check<SB>(buf, sz);
  pointer_check<SB>(static_cast<const void*>(buf), sz);
  pointer_check<SB>(static_cast<const char*>(buf), sz);
  pointer_check<SB>(static_cast<const unsigned char*>(buf), sz);
  pointer_check<SB>(static_cast<const signed char*>(buf), sz);

  GIVEN ("A shared buffer") {
    SB sb(buf, sz);
    REQUIRE (!sb.empty());
    WHEN ("A separate shared buffer is constructed with the buf and size") {
      SB sb2(buf, sz);
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal") {
        REQUIRE (sb == sb2);
      }
    }
    WHEN ("A second shared buffer is copy constructed") {
      SB sb2(sb);
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal") {
        REQUIRE (sb == sb2);
      }
    }
    WHEN ("A shared buffer is constructed from another container") {
      std::list<std::byte> lst (buf, buf+sz);
      SB sb2(lst.begin(), lst.end());
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal") {
        REQUIRE (sb == sb2);
      }
    }
    WHEN ("A separate shared buffer is constructed shorter than the first") {
      std::byte ba[2] = { buf[0], buf[1] };
      SB sb2(ba, 2);
      REQUIRE (!sb2.empty());
      THEN ("the separate shared buffer compares less than the first") {
        REQUIRE (sb2 < sb);
        REQUIRE (sb2 != sb);
      }
    }
    WHEN ("A separate shared buffer is constructed with values less than the first") {
      std::list<std::byte> lst (buf, buf+sz);
      *lst.begin() = 0;
      SB sb2(lst.begin(), lst.end());
      REQUIRE (!sb2.empty());
      THEN ("the separate shared buffer compares less than the first") {
        REQUIRE (sb2 != sb);
      }
    }
  } // end given
}



/*

  GIVEN ("A default constructed shared_buffer") {

    SB sb;
    REQUIRE (sb.empty());

    WHEN ("Resize is called") {
      sb.resize(sz);
      THEN ("the internal buffer should have all zeros") {
        REQUIRE (sb.size() == sz);
        chops::repeat(sz, [&sb] (const int& i) { REQUIRE (*(sb.data() + i) == 0); } );
      }
    }
      THEN ("the two shared buffers compare equal and changes to each are separate") {
        REQUIRE (sb == sb2);
        *sb.data() = Harhar;
        REQUIRE (sb ! = sb2);
      }
      THEN ("the two shared buffers compare equal and a change to the first shows in the second") {
        *sb.data() = Harhar;
        REQUIRE (sb == sb2);
        *sb2.data() = Harhar + 1;
        REQUIRE (sb == sb2);
*/

SCENARIO ( "Const shared buffer common test", "[const_shared_buffer_common]" ) {
  constexpr int Sz = 12;
  std::byte test_buf[Sz] = { 40, 41, 42, 43, 44, 60, 59, 58, 57, 56, 42, 42 };
  shared_buffer_common<chops::const_shared_buffer>(test_buf, Sz);
}

SCENARIO ( "Mutable shared buffer common test", "[mutable_shared_buffer_common]" ) {
  constexpr int Sz = 8;
  std::byte test_buf[Sz] = { 80, 81, 82, 83, 84, 90, 91, 92 };
  shared_buffer_common<chops::const_shared_buffer>(test_buf, Sz);
}

