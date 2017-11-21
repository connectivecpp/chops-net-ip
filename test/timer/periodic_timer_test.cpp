#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <chrono>

#include <thread>

#include "timer/periodic_timer.hpp"
#include "utility/repeat.hpp"

TEST_CASE( "Testing periodic timer", "[periodic_timer]" ) {
  
  SECTION ( "Testing instantiation and basic method operation in non threaded operation" ) {
    chops::wait_queue<int> wq;
    non_threaded_int_test(wq);
  }

  SECTION ( "Testing ring_span instantiation and basic method operation in non threaded operation" ) {
    const int sz = 10;
    int buf[sz];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
    non_threaded_int_test(wq);
  }
  
}

