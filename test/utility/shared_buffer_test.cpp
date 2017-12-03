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
#include "utility/make_byte_array.hpp"

constexpr std::byte Harhar { 42 };
constexpr int N = 11;



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
  const void* vp = static_cast<const void*>(buf);
  pointer_check<SB>(vp, sz);
  pointer_check<SB>(static_cast<const char*>(vp), sz);
  pointer_check<SB>(static_cast<const unsigned char*>(vp), sz);
  pointer_check<SB>(static_cast<const signed char*>(vp), sz);

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
      SB sb2(lst.cbegin(), lst.cend());
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal") {
        REQUIRE (sb == sb2);
      }
    }
    WHEN ("A separate shared buffer is constructed shorter than the first") {
      auto ba = chops::make_byte_array(buf[0], buf[1]);
      SB sb2(ba.cbegin(), ba.cend());
      REQUIRE (!sb2.empty());
      THEN ("the separate shared buffer compares less than the first") {
        REQUIRE (sb2 < sb);
        REQUIRE (sb2 != sb);
      }
    }
    WHEN ("A separate shared buffer is constructed with values less than the first") {
      auto ba = chops::make_byte_array(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
      SB sb2(ba.cbegin(), ba.cend());
      REQUIRE (!sb2.empty());
      THEN ("the separate shared buffer compares less than the first") {
        REQUIRE (sb2 != sb);
      }
    }
  } // end given
}

SCENARIO ( "Const shared buffer common test", "[const_shared_buffer_common]" ) {
  auto arr = chops::make_byte_array( 40, 41, 42, 43, 44, 60, 59, 58, 57, 56, 42, 42 );
  shared_buffer_common<chops::const_shared_buffer>(arr.data(), arr.size());
}

SCENARIO ( "Mutable shared buffer common test", "[mutable_shared_buffer_common]" ) {
  auto arr = chops::make_byte_array ( 80, 81, 82, 83, 84, 90, 91, 92 );
  shared_buffer_common<chops::const_shared_buffer>(arr.data(), arr.size());
}

SCENARIO ( "Mutable shared buffer copy construction and assignment", "[mutable_shared_copy]" ) {

  GIVEN ("A default constructed mutable shared_buffer") {

    auto arr = chops::make_byte_array ( 80, 81, 82, 83, 84, 90, 91, 92 );

    chops::mutable_shared_buffer sb;
    REQUIRE (sb.empty());

    WHEN ("Another mutable shared buffer is assigned into it") {
      chops::mutable_shared_buffer sb2(arr.cbegin(), arr.cend());
      sb = sb2;
      THEN ("the size has changed and the two shared buffers compare equal") {
        REQUIRE (sb.size() == arr.size());
        REQUIRE (sb == sb2);
      }
    }
    WHEN ("Another mutable shared buffer is copy constructed") {
      sb = chops::mutable_shared_buffer(arr.cbegin(), arr.cend());
      chops::mutable_shared_buffer sb2(sb);
      THEN ("the two shared buffers compare equal and a change to the first shows in the second") {
        REQUIRE (sb == sb2);
        *(sb.data()+0) = Harhar;
        *(sb.data()+1) = Harhar;
        REQUIRE (sb == sb2);
      }
    }
  } // end given
}

SCENARIO ( "Mutable shared buffer resize and clear", "[mutable_shared_resize_clear]" ) {

  GIVEN ("A default constructed mutable shared_buffer") {
    chops::mutable_shared_buffer sb;
    WHEN ("Resize is called") {
      sb.resize(N);
      THEN ("the internal buffer will have all zeros") {
        REQUIRE (sb.size() == N);
        chops::repeat(N, [&sb] (const int& i) { REQUIRE (*(sb.data() + i) == std::byte{0} ); } );
      }
    }
    WHEN ("Another mutable shared buffer with a size is constructed") {
      sb.resize(N);
      chops::mutable_shared_buffer sb2(N);
      THEN ("the two shared buffers compare equal, with all zeros in the buffer") {
        REQUIRE (sb == sb2);
        chops::repeat(N, [&sb, &sb2] (const int& i) {
          REQUIRE (*(sb.data() + i) == std::byte{0} );
          REQUIRE (*(sb2.data() + i) == std::byte{0} );
        } );
      }
    }
    WHEN ("The mutable shared buffer is cleared") {
      sb.resize(N);
      sb.clear();
      THEN ("the size will be zero and the buffer is empty") {
        REQUIRE (sb.size() == 0);
        REQUIRE (sb.empty());
      }
    }
  } // end given
}

SCENARIO ( "Mutable shared buffer swap", "[mutable_shared_swap]" ) {

  GIVEN ("Two mutable shared_buffers") {
    auto arr1 = chops::make_byte_array (0xaa, 0xbb, 0xcc);
    auto arr2 = chops::make_byte_array (0x01, 0x02, 0x03, 0x04, 0x05);

    chops::mutable_shared_buffer sb1(arr1.cbegin(), arr1.cend());
    chops::mutable_shared_buffer sb2(arr2.cbegin(), arr2.cend());

    WHEN ("The buffers are swapped") {
      chops::swap(sb1, sb2);
      THEN ("the sizes and contents will be swapped") {
        REQUIRE (sb1.size() == arr2.size());
        REQUIRE (sb2.size() == arr1.size());
        REQUIRE (*(sb1.data()+0) == *(arr2.data()+0));
        REQUIRE (*(sb1.data()+1) == *(arr2.data()+1));
        REQUIRE (*(sb2.data()+0) == *(arr1.data()+0));
        REQUIRE (*(sb2.data()+1) == *(arr1.data()+1));
      }
    }
  } // end given
}

SCENARIO ( "Mutable shared buffer append", "[mutable_shared_append]" ) {

  auto arr = chops::make_byte_array (0xaa, 0xbb, 0xcc);
  auto arr2 = chops::make_byte_array (0xaa, 0xbb, 0xcc, 0xaa, 0xbb, 0xcc);
  chops::mutable_shared_buffer ta(arr.cbegin(), arr.cend());
  chops::mutable_shared_buffer ta2(arr2.cbegin(), arr2.cend());

  GIVEN ("A default constructed mutable shared_buffer") {
    chops::mutable_shared_buffer sb;
    WHEN ("Append with a pointer and size is called") {
      sb.append(arr.data(), arr.size());
      THEN ("the internal buffer will contain the appended data") {
        REQUIRE (sb == ta);
      }
    }
    WHEN ("Append with a mutable shared buffer is called") {
      sb.append(ta);
      THEN ("the internal buffer will contain the appended data") {
        REQUIRE (sb == ta);
      }
    }
    WHEN ("Append is called twice") {
      sb.append(ta);
      sb.append(ta);
      THEN ("the internal buffer will contain twice the appended data") {
        REQUIRE (sb == ta2);
      }
    }
    WHEN ("Appending with single bytes") {
      sb.append(std::byte(0xaa));
      sb.append(std::byte(0xbb));
      sb += std::byte(0xcc);
      THEN ("the internal buffer will contain the appended data") {
        REQUIRE (sb == ta);
      }
    }
  } // end given
}

SCENARIO ( "Compare a mutable shared_buffer with a const shared buffer", "[shared_buffer_compare]" ) {

  GIVEN ("An array of bytes") {
    auto arr = chops::make_byte_array (0xaa, 0xbb, 0xcc);
    WHEN ("A mutable_shared_buffer and a const_shared_buffer are created from the bytes") {
      chops::mutable_shared_buffer msb(arr.cbegin(), arr.cend());
      chops::const_shared_buffer csb(arr.cbegin(), arr.cend());
      THEN ("the shared buffers will compare equal") {
        REQUIRE (msb == csb);
        REQUIRE (csb == msb);
      }
    }
  } // end given
}

SCENARIO ( "Mutable shared buffer move into const shared buffer", "[mutable_shared_move_to_const_shared]" ) {

  auto arr1 = chops::make_byte_array (0xaa, 0xbb, 0xcc);
  auto arr2 = chops::make_byte_array (0x01, 0x02, 0x03, 0x04, 0x05);

  GIVEN ("A mutable_shared_buffer") {
    chops::mutable_shared_buffer msb(arr1.cbegin(), arr1.cend());
    WHEN ("A const_shared_buffer is move constructed from the mutable_shared_buffer") {
      chops::const_shared_buffer csb(std::move(msb));
      THEN ("the const_shared_buffer will contain the data and the mutable_shared_buffer will not") {
        REQUIRE (csb == chops::const_shared_buffer(arr1.cbegin(), arr1.cend()));
        REQUIRE (!(msb == csb));
        msb.clear();
        msb.resize(arr2.size());
        msb.append(arr2.cbegin(), arr2.size());
        REQUIRE (!(msb == csb));
      }
    }
  } // end given
}

