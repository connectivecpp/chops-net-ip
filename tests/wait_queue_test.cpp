#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "wait_queue.hpp"
#include "nonstd/ring_span.hpp"


template <typename Q>
void non_threaded_int_test(Q& wq) {
  const int base = 10;
  wq.push(base+1); wq.push(base+2); wq.push(base+3); wq.push(base+4); 
  REQUIRE (!wq.empty());
  REQUIRE (!wq.is_closed());
  REQUIRE (wq.size() == 4);

  CAPTURE (wq.size());

  int sum = 0;
  wq.apply( [&sum] (const int& i) { sum += i; } );
  REQUIRE (sum == 50);

  REQUIRE (wq.try_pop() == (base+1));
  REQUIRE (wq.size() == 3);
  REQUIRE (wq.try_pop() == (base+2));
  REQUIRE (wq.size() == 2);
  REQUIRE (wq.try_pop() == (base+3));
  REQUIRE (wq.size() == 1);
  REQUIRE (wq.try_pop() == (base+4));
  REQUIRE (wq.size() == 0);
  REQUIRE (wq.empty());

  CAPTURE (wq.size());
}

TEST_CASE( "Testing wait_queue class template", "[wait_queue]" ) {
  
  SECTION ( "Testing instantiation and basic method operation in non threaded operation" ) {
    chops::wait_queue<int> wq;
    non_threaded_int_test(wq);
  }

  SECTION ( "Testing ring_span instantiation and basic method operation in non threaded operation" ) {
    int buf[10];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+9);
    non_threaded_int_test(wq);
  }

}
