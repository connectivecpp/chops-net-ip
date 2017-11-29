#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <experimental/io_context>

#include <chrono>
#include <thread>
#include <optional>
#include <system_error>

#include "timer/periodic_timer.hpp"
#include "utility/repeat.hpp"

const int Expected = 5;
int count = 0;

SCENARIO ( "A periodic timer can be instantiated on the steady clock", "[periodic_timer_steady_clock]" ) {

  GIVEN ( "A steady clock, a duration, and a wait time") {

    std::experimental::net::io_context ioc;
    chops::periodic_timer<std::chrono::steady_clock> timer {ioc};
    std::experimental::net::executor_work_guard<std::experimental::net::io_context::executor_type>
      wg { std::experimental::net::make_work_guard(ioc) };

    std::thread thr([&ioc] () { ioc.run(); } );
    count = 0;

    WHEN ( "The duration is one second and wait time is 6 seconds" ) {

      timer.start_duration_timer( [] (const std::error_code& err, 
                                      const typename std::chrono::steady_clock::duration& elapsed) {
          ++count;
          INFO ("count = " << count << ", err code = " << err.value() << ", " << err.message());
          return count < Expected;
        }, std::chrono::seconds(1));


      std::chrono::seconds wait_time {Expected+1};

      std::this_thread::sleep_for(wait_time);
      wg.reset();
      thr.join();
      INFO ("Thread joined");

      THEN ( "the timer callback count should match expected") {
        REQUIRE (count == Expected);
      }
    }
  } // end given
}

