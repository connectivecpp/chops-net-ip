#include "catch_with_main.hpp"

#include "repeat.hpp"

int gSum = 0;

void myfunc_a () {
  gSum += 1;
}

void myfunc_b ( int i ) {
  gsum += i;
}

TEST_CASE( "Repeat function test", "[repeat]" ) {
  gSum = 0;
  REQUIRE( gSum == 0 );

  const int N = 50;
  const int Sum = N * (N + 1) / 2;
  INFO ("N = " << N << ", Sum = " << Sum);

  SECTION ( "Testing myfunc without index") {
    repeat(N, myfunc_a);
    REQUIRE (gSum == Sum);
  }
  SECTION ( "Testing myfunc with index") {
    repeat(N, myfunc_b);
    REQUIRE (gSum == Sum);
  }
  SECTION ( "Testing lambda func without index") {
    repeat(N, [] { myfunc_a() } );
    REQUIRE (gSum == Sum);
  }
  SECTION ( "Testing lambda func with index") {
    repeat(N, [] { myfunc_a(int i) } );
    REQUIRE (gSum == Sum);
  }
  SECTION ( "Testing lambda func without index and local var") {
    int lSum = 0;
    repeat(N, [&lSum] { lSum += 1; } );
    REQUIRE (lSum == Sum);
  }
  SECTION ( "Testing lambda func with index and local var") {
    int lSum = 0;
    repeat(N, [&lSum] (int i) { lSum += i; } );
    REQUIRE (lSum == Sum);
  }

}
