#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <utility> // std::pair
#include <functional> // std::ref
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <chrono>

#include <thread>
#include <mutex>

#include "utility/wait_queue.hpp"
#include "utility/repeat.hpp"

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


template <typename Q, typename T>
void read_func (Q& wq, std::set<std::pair< int, T> >& s, std::mutex& mut) {
  while (true) {
    std::optional<std::pair<int, T> > opt_elem = wq.wait_and_pop();
    if (!opt_elem) { // empty element means close has been called
      return;
    }
    std::lock_guard<std::mutex> lk(mut);
    s.insert(*opt_elem);
  }
}

template <typename Q, typename T>
void write_func (Q& wq, int start, int slice, const T& val) {
  chops::repeat (slice, [&wq, start, val] (const int& i) {
      std::pair<int, T> elem((start+i), val);
      if (!wq.push(elem)) {
        FAIL("wait queue push failed in write_func");
      }
    }
  );
}

template <typename Q, typename T>
void threaded_test(Q& wq, int num_readers, int num_writers, int slice, const T& val) {
  // each writer pushes slice entries
  int tot = num_writers * slice;

  std::set<std::pair< int, T> > s;
  std::mutex mut;

  std::vector<std::thread> rd_thrs;
  chops::repeat(num_readers, [&wq, &rd_thrs, &s, &mut] {
      rd_thrs.push_back( std::thread (read_func<Q, T>, std::ref(wq), std::ref(s), std::ref(mut)) );
    }
  );

  std::vector<std::thread> wr_thrs;
  chops::repeat(num_writers, [&wq, &wr_thrs, slice, val] (const int& i) {
      wr_thrs.push_back( std::thread (write_func<Q, T>, std::ref(wq), (i*slice), slice, std::cref(val)));
    }
  );
  // wait for writers to finish pushing vals
  for (auto& thr : wr_thrs) {
    thr.join();
  }
  // sleep and loop waiting for wait queue to be emptied by reader threads
  while (!wq.empty()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  wq.close();

  // wait for readers; since wait queue is empty and closed they should all join immediately
  for (auto& thr : rd_thrs) {
    thr.join();
  }
  REQUIRE (wq.empty());
  REQUIRE (wq.is_closed());
  // check set to make sure all entries are present
  REQUIRE (s.size() == tot);
  int idx = 0;
  for (const auto& e : s) {
    REQUIRE (e.first == idx);
    REQUIRE (e.second == val);
    ++idx;
  }
  INFO ("Set looks good!");

}

TEST_CASE( "Testing wait_queue class template", "[wait_queue_deque]" ) {
  
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
  
  SECTION ( "Testing copy construction or move construction only element type requirements" ) {

    struct Foo {
      Foo() = delete;
      Foo(double x) : doobie(x) { }
      Foo(const Foo&) = default;
      Foo(Foo&&) = delete;
      Foo& operator=(const Foo&) = delete;
      Foo& operator=(Foo&&) = delete;
      double doobie;
    };

    chops::wait_queue<Foo> wqfoo;
    Foo answer {42.0};
    wqfoo.push(answer);
    std::optional<Foo> foo { wqfoo.try_pop() };
    REQUIRE (wqfoo.empty());
    REQUIRE ((*foo).doobie == 42.0);

    struct Bar {
      Bar() = delete;
      Bar(double x) : doobie(x) { }
      Bar(const Bar&) = delete;
      Bar(Bar&&) = default;
      Bar& operator=(const Bar&) = delete;
      Bar& operator=(Bar&&) = delete;
      double doobie;
    };

    chops::wait_queue<Bar> wqbar;
    wqbar.push(Bar{42.0});
    std::optional<Bar> bar { wqbar.try_pop() };
    REQUIRE (wqbar.empty());
    REQUIRE ((*bar).doobie == 42.0);
  }
}

TEST_CASE( "Testing ring_span roll around inside wait queue", "[wait_queue_roll_around]" ) {
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

TEST_CASE( "Threaded wait queue test small numbers", "[wait_queue_threaded_small]" ) {
  SECTION ( "Threaded test with deque int, 1 reader and 1 writer thread, 100 slice" ) {
    chops::wait_queue<std::pair<int, int> > wq;
    threaded_test(wq, 1, 1, 100, 44);
  }
  SECTION ( "Threaded test with deque int, 5 reader and 3 writer threads, 1000 slice" ) {
    chops::wait_queue<std::pair<int, int> > wq;
    threaded_test(wq, 5, 3, 1000, 1212);
  }
  SECTION ( "Threaded test with deque int, 60 reader and 40 writer threads, 5000 slice" ) {
    chops::wait_queue<std::pair<int, int> > wq;
    threaded_test(wq, 60, 40, 5000, 5656);
  }
  SECTION ( "Threaded test with deque string, 60 reader and 40 writer threads, 12000 slice" ) {
    using namespace std::literals::string_literals;
    chops::wait_queue<std::pair<int, std::string> > wq;
    threaded_test(wq, 60, 40, 12000, "cool, lit, sup"s);
  }
}
/*
TEST_CASE( "Threaded wait queue test big numbers", "[wait_queue_threaded_big]" ) {
  SECTION ( "Threaded test with deque int, 500 reader and 300 writer threads, 50000 slice" ) {
    chops::wait_queue<std::pair<int, int> > wq;
    threaded_test(wq, 500, 300, 50000, 7777);
  }
}
*/
