#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "repeat.hpp"

int gSum = 0;

void myfunc_a () {
  gSum += 1;
}

void myfunc_b ( int i ) {
  REQUIRE (gSum == i);
  gSum += 1;
}

TEST_CASE( "Testing repeat function template", "[repeat]" ) {
  gSum = 0;
  REQUIRE( gSum == 0 );

  const int N = 50;
  CAPTURE (N);

  SECTION ( "Testing myfunc without index") {
    chops::repeat(N, myfunc_a);
    REQUIRE (gSum == N);
  }
  SECTION ( "Testing myfunc with index") {
    chops::repeat(N, myfunc_b);
    REQUIRE (gSum == N);
  }
  SECTION ( "Testing lambda func without index") {
    chops::repeat(N, [] { myfunc_a(); } );
    REQUIRE (gSum == N);
  }
  SECTION ( "Testing lambda func with index") {
    chops::repeat(N, [] (int i) { myfunc_b(i); } );
    REQUIRE (gSum == N);
  }
  SECTION ( "Testing lambda func without index and local var") {
    int lSum = 0;
    chops::repeat(N, [&lSum] { lSum += 1; } );
    REQUIRE (lSum == N);
  }
  SECTION ( "Testing lambda func with index and local var") {
    int lSum = 0;
    chops::repeat(N, [&lSum] (int i) { REQUIRE (lSum == i); lSum += 1; } );
    REQUIRE (lSum == N);
  }

}
