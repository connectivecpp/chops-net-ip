#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "repeat.hpp"

void repeater ( int N ) {

  int sum = 0;


}

void myfunc ( int i ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}


TEST_CASE( "Repeat function is called" << N << "times", "[repeat]" ) {
    REQUIRE( Factorial(1) == 1 );
    REQUIRE( Factorial(2) == 2 );
    REQUIRE( Factorial(3) == 6 );
    REQUIRE( Factorial(10) == 3628800 );
}
