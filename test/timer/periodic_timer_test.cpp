#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <experimental/io_context>

#include <chrono>
#include <thread>

#include "timer/periodic_timer.hpp"
#include "utility/repeat.hpp"

template <typename Clock, typename Dur, typename TP>
void timer_test (int expected, Dur timer_dur, Dur wait_time, TP start_time) {

  chops::periodic_timer<Clock> timer;
  std::experimental::net::io_context ioc;
  std::experimental::net::executor_work_guard<std::experimental::net::io_context::executor_type>
    wg { std::experimental::net::make_work_guard(ioc) };

  std::thread thr([&ioc] () { ioc.run(); } );
  
  int count = 0;
  timer.start([&count] (const std::system_error& err, const Dur& elapsed) {
      ++count;
      INFO ("count  " << count << ", sys err = " << err.what());
    }
  );
  std::this_thread::sleep_for(wait_time);
  wg.reset();
  thr.join();
  INFO ("Thread joined");
  REQUIRE (count == expected);
}

TEST_CASE( "Testing periodic timer", "[periodic_timer]" ) {
  
}

