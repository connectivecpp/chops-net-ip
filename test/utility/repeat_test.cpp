/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for Vittorio Romeo's repeat utility function.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include "utility/repeat.hpp"

int gSum = 0;

void myfunc_a () {
  gSum += 1;
}

void myfunc_b ( int i ) {
  REQUIRE (gSum == i);
  gSum += 1;
}

SCENARIO( "Vittorio Romeo's repeat utility is a function template to repeat code N times", "[repeat]" ) {
  constexpr int N = 50;
  INFO("N = " << N);
  GIVEN ("A global counter set to 0 and an iteration count set to N") {
  }
  WHEN ("A function that doesn't care about the passed in index is invoked") {
    gSum = 0;
    chops::repeat(N, myfunc_a);
    THEN ("the global counter should now equal N") {
      REQUIRE (gSum == N);
    }
  }
  AND_WHEN ("A function that does care about the passed in index is invoked") {
    gSum = 0;
    chops::repeat(N, myfunc_b);
    THEN ("the global counter should now equal N") {
      REQUIRE (gSum == N);
    }
  }
  AND_WHEN ("A lambda func that doesn't care about the passed in index is invoked") {
    gSum = 0;
    chops::repeat(N, [] { myfunc_a(); } );
    THEN ("the global counter should now equal N") {
      REQUIRE (gSum == N);
    }
  }
  AND_WHEN ("A lambda func that does care about the passed in index is invoked") {
    gSum = 0;
    chops::repeat(N, [] (int i) { myfunc_b(i); } );
    THEN ("the global counter should now equal N") {
      REQUIRE (gSum == N);
    }
  }
  AND_WHEN ("A lambda func that doesn't care about the index but has a local var is invoked") {
    int lSum = 0;
    chops::repeat(N, [&lSum] { lSum += 1; } );
    THEN ("the local counter should now equal N") {
      REQUIRE (lSum == N);
    }
  }
  AND_WHEN ("A lambda func that does care about the index and has a local var is invoked") {
    int lSum = 0;
    chops::repeat(N, [&lSum] (int i) { REQUIRE (lSum == i); lSum += 1; } );
    THEN ("the local counter should now equal N") {
      REQUIRE (lSum == N);
    }
  }
}
