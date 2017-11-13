#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <utility> // std::pair
#include <vector>
#include <set>
#include <optional>

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


template <typename T>
using element_type = std::pair<int, T>;
template <typename T>
using opt_element_type = std::optional<element_type<T> >;
template <typename T>
using set_type = std::set<opt_element_type<T> >;

template <typename Q, typename T>
void read_func (Q& wq, set_type<T>& s) {
  bool looping = true;
  while (looping) {
    opt_element_type<T> elem = wq.wait_and_pop();
    if (elem) {
      s.insert(*elem);
    }
    else {
      looping = false; // empty element means close has been called
    }
  }
}

void write_func () {
  // need stuff here
}

template <typename Q, typename T>
void threaded_test(Q& wq, int num_readers, int num_writers, int slice, const T& val) {
  // each writer pushes slice entries
  int tot = num_writers * slice;

  set_type<T> s;

  using std::vector<std::thread> thr_vec_type;

  thr_vec_type rd_thrs;
  chops::repeat(num_readers, [wq, &rd_thrs, &s] { rd_thrs.push_back( std::thread (read_func, wq, s); } );

  thr_vec_type wr_thrs;
  chops::repeat(num_writers, [wq, &wr_thrs, &s] { wr_thrs.push_back( std::thread (write_func, wq, s); } );


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
