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

#include "queue/wait_queue.hpp"
#include "utility/repeat.hpp"

#include "nonstd/ring_span.hpp"

using namespace std::literals::string_literals;

template <typename T, typename Q>
void non_threaded_push_test(Q& wq, T val, int count) {

  GIVEN ("A newly constructed wait_queue") {
    REQUIRE (wq.empty());
    REQUIRE (wq.size() == 0);

    WHEN ("Values are pushed on the queue") {
      chops::repeat(count, [&wq, val] () { REQUIRE(wq.push(val)); } );
      THEN ("the size is increased") {
        REQUIRE (!wq.empty());
        REQUIRE (wq.size() == count);
      }
    }

    WHEN ("Values are popped from the queue") {
      chops::repeat(count, [&wq, val] () { wq.push(val); } );
      chops::repeat(count, [&wq, val] () { auto ret = wq.try_pop(); REQUIRE(*ret == val); } );
      THEN ("the size decreases to zero") {
        REQUIRE (wq.empty());
        REQUIRE (wq.size() == 0);
      }
    }
  } // end given
}

template <typename T, typename Q>
void non_threaded_arithmetic_test(Q& wq, T base_val, int count, T expected_sum) {

  GIVEN ("A newly constructed wait_queue, which will have numeric values added") {
    REQUIRE (wq.empty());

    WHEN ("Apply is called against all elements to compute a sum") {
      chops::repeat(count, [&wq, base_val] (const int& i) { REQUIRE(wq.push(base_val+i)); } );
      T sum { 0 };
      wq.apply( [&sum] (const int& i) { sum += i; } );
      THEN ("the sum should match the expected sum") {
        REQUIRE (sum == expected_sum);
      }
    }

    WHEN ("Try_pop is called") {
      chops::repeat(count, [&wq, base_val] (const int& i) { wq.push(base_val+i); } );
      THEN ("elements should be popped in FIFO order") {
        chops::repeat(count, [&wq, base_val] (const int& i) { REQUIRE(*(wq.try_pop()) == (base_val+i)); } );
        REQUIRE (wq.size() == 0);
        REQUIRE (wq.empty());
      }
    }

  } // end given

}

template <typename T, typename Q>
void non_threaded_open_close_test(Q& wq, T val, int count) {

  GIVEN ("A newly constructed wait_queue") {

    REQUIRE (!wq.is_closed());

    WHEN ("Close is called") {
      wq.close();
      THEN ("the state is now closed, and pushes fail") {
        REQUIRE (wq.is_closed());
        REQUIRE (!wq.push(val));
        REQUIRE (wq.empty());
      }
    }
    WHEN ("Open is called") {
      wq.close();
      REQUIRE (wq.is_closed());
      wq.open();
      THEN ("the state is now open, and pushes will succeed") {
        REQUIRE (!wq.is_closed());
        REQUIRE (wq.empty());
        chops::repeat(count, [&wq, val] () { wq.push(val); } );
        REQUIRE (wq.size() == count);
      }
    }
    WHEN ("Close is called") {
      chops::repeat(count, [&wq, val] () { wq.push(val); } );
      REQUIRE (!wq.empty());
      wq.close();
      THEN ("wait_and_pops will not return data, but try_pops will") {
        auto ret = wq.wait_and_pop();
        REQUIRE (!ret);
        ret = wq.wait_and_pop();
        REQUIRE (!ret);
        chops::repeat(count, [&wq, &ret] () { ret = wq.try_pop(); REQUIRE(ret); } );
        REQUIRE (wq.empty());
        ret = wq.try_pop();
        REQUIRE (!ret);
      }
    }

  } // end given
}

template <typename T, typename Q>
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

template <typename T, typename Q>
void write_func (Q& wq, int start, int slice, const T& val) {
  chops::repeat (slice, [&wq, start, &val] (const int& i) {
      if (!wq.push(std::pair<int, T>{(start+i), val})) {
        FAIL("wait queue push failed in write_func");
      }
    }
  );
}

template <typename T, typename Q>
void threaded_test(Q& wq, int num_readers, int num_writers, int slice, const T& val) {
  // each writer pushes slice entries
  int tot = num_writers * slice;

  std::set<std::pair< int, T> > s;
  std::mutex mut;

  std::vector<std::thread> rd_thrs;
  chops::repeat(num_readers, [&wq, &rd_thrs, &s, &mut] {
      rd_thrs.push_back( std::thread (read_func<T, Q>, std::ref(wq), std::ref(s), std::ref(mut)) );
    }
  );

  std::vector<std::thread> wr_thrs;
  chops::repeat(num_writers, [&wq, &wr_thrs, slice, &val] (const int& i) {
      wr_thrs.push_back( std::thread (write_func<T, Q>, std::ref(wq), (i*slice), slice, val));
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

// TODO - figure out command line parsing to pass in counts
const int N = 10;

SCENARIO ( "A wait_queue can be used as a FIFO, with element type int and the default container type of std::deque" ) {
  chops::wait_queue<int> wq;
  non_threaded_push_test(wq, 42, N);
  non_threaded_arithmetic_test(wq, 0, N, 45);
  non_threaded_open_close_test(wq, 42, N);
}

SCENARIO ( "A wait_queue can be used as a FIFO, with element type double and the default container type of std::deque" ) {
  chops::wait_queue<double> wq;
  non_threaded_push_test(wq, 42.0, N);
  non_threaded_arithmetic_test(wq, 0.0, N, 45.0);
  non_threaded_open_close_test(wq, 42.0, N);
}

SCENARIO ( "A wait_queue can be used as a FIFO, with element type int and the default container type of std::deque" ) {
    int buf[sz];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
  non_threaded_push_test<int, std::deque<int> >(42, N);
  non_threaded_arithmetic_test<int, std::deque<int> >(0, N, 45);
  non_threaded_open_close_test<int, std::deque<int> >(42, N);
}
  
#if 0

TEST_CASE( "Testing wait_queue class template", "[wait_queue_deque]" ) {
  
  SECTION ( "Testing instantiation and basic method operation in non threaded operation" ) {
    non_threaded_push_test<int, std::deque<int> >();
  }

  SECTION ( "Testing ring_span instantiation and basic method operation in non threaded operation" ) {
    const int sz = 10;
    int buf[sz];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
    non_threaded_test(wq);
  }
  
  SECTION ( "Testing copy construction, move construction, emplacement" ) {

  {
    struct Foo {
      Foo() = delete;
      Foo(double x) : doobie(x) { }
      Foo(const Foo&) = default;
      Foo(Foo&&) = delete;
      Foo& operator=(const Foo&) = delete;
      Foo& operator=(Foo&&) = delete;
      double doobie;
    };

    chops::wait_queue<Foo> wq;
    Foo answer {42.0};
    wq.push(answer);
    std::optional<Foo> foo { wq.try_pop() };
    REQUIRE (wq.empty());
    REQUIRE ((*foo).doobie == 42.0);
  }

  {
    struct Bar {
      Bar() = delete;
      Bar(double x) : doobie(x) { }
      Bar(const Bar&) = delete;
      Bar(Bar&&) = default;
      Bar& operator=(const Bar&) = delete;
      Bar& operator=(Bar&&) = delete;
      double doobie;
    };

    chops::wait_queue<Bar> wq;
    wq.push(Bar{42.0});
    std::optional<Bar> bar { wq.try_pop() };
    REQUIRE (wq.empty());
    REQUIRE ((*bar).doobie == 42.0);
  }

  {
    struct Band {
      Band() = delete;
      Band(double x, const std::string& bros) : doobie(x), brothers(bros), engagements() {
        engagements = {"Seattle"s, "Portland"s, "Boise"s};
      }
      Band(const Band&) = delete;
      Band(Band&&) = default;
      Band& operator=(const Band&) = delete;
      Band& operator=(Band&&) = delete;
      double doobie;
      std::string brothers;
      std::vector<std::string> engagements;
    };

    chops::wait_queue<Band> wq;
    wq.push(Band{42.0, "happy"s});
    wq.emplace_push(44.0, "sad"s);
    std::optional<Band> val1 { wq.try_pop() };
    REQUIRE ((*val1).doobie == 42.0);
    REQUIRE ((*val1).brothers == "happy"s);
    std::optional<Band> val2 { wq.try_pop() };
    REQUIRE ((*val2).doobie == 44.0);
    REQUIRE ((*val2).brothers == "sad"s);
    REQUIRE (wq.empty());
  }

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
#endif

