/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c periodic_timer class template.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <experimental/io_context>

#include <chrono>
#include <thread>
#include <optional>
#include <system_error>

#include "timer/periodic_timer.hpp"
#include "utility/repeat.hpp"

const int Expected = 9;
int count = 0;

bool lambda_util (const std::error_code& err) {
  ++count;
  INFO ("count = " << count << ", err code = " << err.value() << ", " << err.message());
  return count < Expected;
}

using wk_guard = std::experimental::net::executor_work_guard<std::experimental::net::io_context::executor_type>;

void wait_util (int wait, wk_guard& wg, std::thread& thr) {
  std::this_thread::sleep_for(std::chrono::seconds(wait));
  wg.reset();
  thr.join();
  INFO ("Thread joined");
}


template <typename Clock>
void test_util () {

  GIVEN ( "A clock and a duration") {

    std::experimental::net::io_context ioc;
    chops::periodic_timer<Clock> timer {ioc};
    wk_guard wg { std::experimental::net::make_work_guard(ioc) };

    std::thread thr([&ioc] () { ioc.run(); } );
    count = 0;

    WHEN ( "The duration is one second" ) {
      timer.start_duration_timer( [] (const std::error_code& err, const typename Clock::duration& elap) { 
          return lambda_util(err);
        } , std::chrono::seconds(1));

      wait_util (Expected+1, wg, thr);

      THEN ( "the timer callback count should match expected") {
        REQUIRE (count == Expected);
      }
    }
    WHEN ( "The duration is two seconds and the start time is 5 seconds in the future" ) {
      timer.start_duration_timer( [] (const std::error_code& err, const typename Clock::duration& elap) { 
          return lambda_util(err);
        } , std::chrono::seconds(2), Clock::now() + std::chrono::seconds(5));

      wait_util(Expected * 2 + 5 + 1, wg, thr);

      THEN ( "the timer callback count should match expected") {
        REQUIRE (count == Expected);
      }
    }
    WHEN ( "The duration is one second and the timer pops on timepoints" ) {
      timer.start_timepoint_timer( [] (const std::error_code& err, const typename Clock::duration& elap) { 
          return lambda_util(err);
        } , std::chrono::seconds(1));

      wait_util (Expected+1, wg, thr);

      THEN ( "the timer callback count should match expected") {
        REQUIRE (count == Expected);
      }
    }
    WHEN ( "The duration is two seconds and the timer pops on timepoints starting 5 seconds in the future" ) {
      timer.start_timepoint_timer( [] (const std::error_code& err, const typename Clock::duration& elap) { 
          return lambda_util(err);
        } , std::chrono::seconds(2), Clock::now() + std::chrono::seconds(5));

      wait_util(Expected * 2 + 5 + 1, wg, thr);

      THEN ( "the timer callback count should match expected") {
        REQUIRE (count == Expected);
      }
    }

  } // end given
}

SCENARIO ( "A periodic timer can be instantiated on the steady clock", "[periodic_timer_steady_clock]" ) {

  test_util<std::chrono::steady_clock>();

}
SCENARIO ( "A periodic timer can be instantiated on the system clock", "[periodic_timer_system_clock]" ) {

  test_util<std::chrono::system_clock>();

}
SCENARIO ( "A periodic timer can be instantiated on the high resolution clock", "[periodic_timer_high_resolution_clock]" ) {

  test_util<std::chrono::high_resolution_clock>();

}
