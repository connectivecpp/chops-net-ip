/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c mutable_shared_buffer and
 *  @c const_shared_buffer classes.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#define CATCH_CONFIG_MAIN

#include "catch.hpp"


#include <cstddef> // std::byte

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"

constexpr std::byte Harhar { 42 };

template <typename SB, typename TS>
void pointer_check(const T* bp, typename SB::size_type sz) {

  GIVEN ("A buf pointer and a size") {
    SB sb(bp, sz);

    WHEN ("A shared buffer is constructed with the buf and size") {
      THEN ("the shared buffer is not empty, the size matches and the contents match") {
        REQUIRE (!sb.empty());
        REQUIRE (sb.size() == sz);
        const std::byte* buf = static_cast<const std::byte*>(static_cast<const void*>(bp));
        chops::repeat(sz, [&sb, buf] (const int& i) { REQUIRE(*(sb.data()+i) == *(buf+i)); } );
      }
    }
    WHEN ("A separate hared buffer is constructed with the buf and size") {
      SB sb2(bp, sz);
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal and changes to each are separate") {
        REQUIRE (sb == sb2);
        *sb.data() = Harhar;
        REQUIRE (sb ! = sb2);
      }
    }
    WHEN ("A second shared buffer is copy constructed") {
      SB sb2(sb);
      REQUIRE (!sb2.empty());
      THEN ("the two shared buffers compare equal and a change to the first shows in the second") {
        *sb.data() = Harhar;
        REQUIRE (sb == sb2);
        *sb2.data() = Harhar + 1;
        REQUIRE (sb == sb2);
      }
    }
  } // end given
}

template <typename SB>
void shared_buffer_common(const std::byte* buf, typename SB::size_type sz) {
  pointer_check<SB>(buf, sz);
  pointer_check<SB>(static_cast<const void*>(buf), sz);
  pointer_check<SB>(static_cast<const char*>(buf), sz);
  pointer_check<SB>(static_cast<const unsigned char*>(buf), sz);
  pointer_check<SB>(static_cast<const signed char*>(buf), sz);

  GIVEN ("A buf pointer and a size") {

}




  GIVEN ("A default constructed shared_buffer") {

    SB sb;
    REQUIRE (sb.empty());

    WHEN ("Resize is called") {
      sb.resize(sz);
      THEN ("the internal buffer should have all zeros") {
        REQUIRE (sb.size() == sz);
        chops::repeat(sz, [&sb] (const int& i) { REQUIRE (*(sb.data() + i) == 0); } );
      }
    }

constexpr int N = 40;
template <typename T>
constexpr T ExpectedSum = (N / 2) * (N - 1);

SCENARIO ( "Non-threading wait_queue test, with element type int and default container type of std::deque",
           "[wait_queue_int_def_container]" ) {
  chops::wait_queue<int> wq;
  non_threaded_push_test(wq, 42, N);
  non_threaded_arithmetic_test(wq, 0, N, ExpectedSum<int>);
  non_threaded_open_close_test(wq, 42, N);
}

SCENARIO ( "Non-threading wait_queue test, with element type double and default container type of std::deque",
           "[wait_queue_double_def_container]" ) {
  chops::wait_queue<double> wq;
  non_threaded_push_test(wq, 42.0, N);
  non_threaded_arithmetic_test(wq, 0.0, N, ExpectedSum<double>);
  non_threaded_open_close_test(wq, 42.0, N);
}

SCENARIO ( "Non-threading wait_queue test, with element type int and ring_span container type",
           "[wait_queue_int_ring_span_container]" ) {
  int buf[N];
  chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+N);
  non_threaded_push_test(wq, 42, N);
  non_threaded_arithmetic_test(wq, 0, N, ExpectedSum<int>);
  non_threaded_open_close_test(wq, 42, N);
}

SCENARIO ( "Non-threading wait_queue test, with element type std::string and default container type of std::deque",
           "[wait_queue_string_def_container]" ) {
  chops::wait_queue<std::string> wq;
  non_threaded_push_test(wq, "Howzit going, bro!"s, N);
  non_threaded_open_close_test(wq, "It's hanging, bro!"s, N);
}

SCENARIO ( "Non-threading wait_queue test, with element type std::string and ring_span container type",
           "[wait_queue_string_ring_span_container]" ) {
  std::string buf[N];
  chops::wait_queue<std::string, nonstd::ring_span<std::string> > wq(buf+0, buf+N);
  non_threaded_push_test(wq, "No bro speak, please"s, N);
  non_threaded_open_close_test(wq, "Why so serious, bro?"s, N);
}

SCENARIO ( "Non-threading wait_queue test, testing copy construction without move construction",
           "[wait_queue_copy_no_move]" ) {

  struct Foo {
    Foo() = delete;
    Foo(double x) : doobie(x) { }
    Foo(const Foo&) = default;
    Foo(Foo&&) = delete;
    Foo& operator=(const Foo&) = default;
    Foo& operator=(Foo&&) = delete;

    double doobie;

    bool operator==(const Foo& rhs) const { return doobie == rhs.doobie; }
  };

  chops::wait_queue<Foo> wq;
  non_threaded_push_test(wq, Foo(42.0), N);
  non_threaded_open_close_test(wq, Foo(42.0), N);
}

SCENARIO ( "Non-threading wait_queue test, testing move construction without copy construction",
           "[wait_queue_move_no_copy]" ) {

  GIVEN ("A newly constructed wait_queue with a move-only type") {

    struct Bar {
      Bar() = delete;
      Bar(double x) : doobie(x) { }
      Bar(const Bar&) = delete;
      Bar(Bar&&) = default;
      Bar& operator=(const Bar&) = delete;
      Bar& operator=(Bar&&) = default;

      double doobie;

      bool operator==(const Bar& rhs) const { return doobie == rhs.doobie; }
    };

    chops::wait_queue<Bar> wq;
    WHEN ("Values are pushed on the queue") {
      wq.push(Bar(42.0));
      wq.push(Bar(52.0));
      THEN ("the values are moved through the wait_queue") {
        REQUIRE (wq.size() == 2);
        auto ret1 { wq.try_pop() };
        REQUIRE (*ret1 == Bar(42.0));
        auto ret2 { wq.try_pop() };
        REQUIRE (*ret2 == Bar(52.0));
        REQUIRE (wq.empty());
      }
    }
  } // end given
}

SCENARIO ( "Non-threading wait_queue test, testing complex constructor and emplacement",
           "[wait_queue_complex_type]" ) {

  GIVEN ("A newly constructed wait_queue with a more complex type") {

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
    REQUIRE (wq.size() == 0);
    wq.push(Band{42.0, "happy"s});
    wq.emplace_push(44.0, "sad"s);

    WHEN ("Values are emplace pushed on the queue") {
      THEN ("the size is increased") {
        REQUIRE (!wq.empty());
        REQUIRE (wq.size() == 2);
      }
    }

    WHEN ("Values are popped from the queue") {
      std::optional<Band> val1 { wq.try_pop() };
      std::optional<Band> val2 { wq.try_pop() };
      THEN ("the values are correct and the wait_queue is empty") {
        REQUIRE ((*val1).doobie == 42.0);
        REQUIRE ((*val1).brothers == "happy"s);
        REQUIRE ((*val2).doobie == 44.0);
        REQUIRE ((*val2).brothers == "sad"s);
        REQUIRE (wq.empty());
      }
    }
  } // end given
}

SCENARIO ( "Fixed size ring_span, testing wrap around with int type",
           "[wait_queue_int_ring_span_wrap_around]" ) {

  GIVEN ("A newly constructed wait_queue using a ring_span") {

    int buf[N];
    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+N);

    const int Answer = 42;
    const int AnswerPlus = 42+5;

    WHEN ("The wait_queue is loaded completely with answer") {
      chops::repeat(N, [&wq, Answer] { wq.push(Answer); } );
      THEN ("the size is full and all values match answer") {
        REQUIRE (wq.size() == N);
        wq.apply([Answer] (const int& i) { REQUIRE(i == Answer); } );
      }
    }
    WHEN ("The wait_queue is loaded completely with answer, then answer plus is added") {
      chops::repeat(N, [&wq, Answer] { wq.push(Answer); } );
      chops::repeat(N / 2, [&wq, AnswerPlus] { wq.push(AnswerPlus); } );
      THEN ("the size is full but half match answer and half answer plus, since there's been wrap") {
        REQUIRE (wq.size() == N);
        // wait_pop should immediately return if the queue is non empty
        chops::repeat(N / 2, [&wq, Answer] { REQUIRE (wq.wait_and_pop() == Answer); } );
        chops::repeat(N / 2, [&wq, AnswerPlus] { REQUIRE (wq.wait_and_pop() == AnswerPlus); } );
        REQUIRE (wq.empty());
      }
    }
  } // end given
}

SCENARIO ( "Threaded wait queue, deque int", "[wait_queue_threaded_deque_int]" ) {

  GIVEN ("A newly constructed wait_queue, deque int") {
    chops::wait_queue<std::pair<int, int> > wq;

    WHEN ("Parameters are 1 reader, 1 writer thread, 100 slice") {
      THEN ("threads will be created and joined") {
        REQUIRE ( threaded_test(wq, 1, 1, 100, 44) );
      }
    }

    WHEN ("Parameters are 5 reader, 3 writer threads, 1000 slice") {
      THEN ("threads will be created and joined") {
        REQUIRE ( threaded_test(wq, 5, 3, 1000, 1212) );
      }
    }

    WHEN ("Parameters are 60 reader, 40 writer threads, 5000 slice") {
      THEN ("threads will be created and joined") {
        REQUIRE ( threaded_test(wq, 60, 40, 5000, 5656) );
      }
    }
  } // end given
}

SCENARIO ( "Threaded wait queue, deque string", "[wait_queue_threaded_deque_string]" ) {

  GIVEN ("A newly constructed wait_queue, deque string") {
    chops::wait_queue<std::pair<int, std::string> > wq;

    WHEN ("Parameters are 60 reader, 40 writer threads, 12000 slice") {
      THEN ("threads will be created and joined") {
        REQUIRE ( threaded_test(wq, 60, 40, 12000, "cool, lit, sup"s) );
      }
    }
  } // end given
}

