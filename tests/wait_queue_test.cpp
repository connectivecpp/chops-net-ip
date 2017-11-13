#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "wait_queue.hpp"
#include "nonstd/ring_span.hpp"
#include "repeat.hpp"


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
    const int sz = 10;
    int buf[sz];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
    non_threaded_int_test(wq);
  }

}

TEST_CASE( "Testing ring_span roll around inside wait queue", "[wait_queue ring_span roll around]" ) {
    const int sz = 20;
    const int answer = 42;
    const int answer_plus = 42+5;
    int buf[sz];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
    chops::repeat(sz, [&wq, answer] { wq.push(answer); } );
    REQUIRE (wq.size() == sz);
    wq.apply([answer] (const int& i) { REQUIRE(i == answer); } );
    chops::repeat(sz / 2, [&wq, answer] { wq.push(answer_plus); } );
    REQUIRE (wq.size() == sz);
    // wait_pop should immediately return if the queue is non empty
    chops::repeat(sz / 2, [&wq, answer] { REQUIRE (wq.wait_and_pop() == answer); } );
    chops::repeat(sz / 2, [&wq, answer] { REQUIRE (wq.wait_and_pop() == answer_plus); } );
    REQUIRE (wq.empty());

}
