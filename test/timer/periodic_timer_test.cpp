#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <experimental/io_context>

#include <chrono>
#include <thread>
#include <optional>
#include <system_error>

#include "timer/periodic_timer.hpp"
#include "utility/repeat.hpp"

template <typename Clock>
struct timer_cb {
  int count = 0;
  void operator() (const std::error_code& err, const typename Clock::duration& elapsed) {
    ++count;
    INFO ("Now time = " << Clock::now() << ", count = " << count << ", elapsed = " << elapsed << 
      ", err code = " << err.value() << ", " << err.message());
  }
};


template <typename Clock>
int timer_test (typename Clock::duration timer_dur, typename Clock::duration wait_time,
                std::optional<typename Clock::time_point> start_time, bool dur_timer) {

  chops::periodic_timer<Clock> timer;
  std::experimental::net::io_context ioc;
  std::experimental::net::executor_work_guard<std::experimental::net::io_context::executor_type>
    wg { std::experimental::net::make_work_guard(ioc) };

  std::thread thr([&ioc] () { ioc.run(); } );
  
  int count = 0;
  if (dur_timer) {
    if (start_time) {
      timer.start_duration_timer(timer_cb<Clock>(), timer_dur, start_time);
    }
    else {
      timer.start_duration_timer(timer_cb<Clock>(), timer_dur);
    }
  }
  else {
    if (start_time) {
      timer.start_timepoint_timer(timer_cb<Clock>(), timer_dur, start_time);
    }
    else {
      timer.start_timepoint_timer(timer_cb<Clock>(), timer_dur);
    }
  }
  std::this_thread::sleep_for(wait_time);
  wg.reset();
  thr.join();
  INFO ("Thread joined");
  return count;
}

SCENARIO ( "Periodic timers can be started to callback on durations", "[periodic_timer_duration]" ) {
  GIVEN ( "A duration and ")
  
  REQUIRE (count == expected);
}

