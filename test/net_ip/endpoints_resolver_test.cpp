/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c make_endpoints functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/io_context>

#include <system_error> // std::error_code
#include <utility> // std::pair
#include <future>
#include <string_view>

#include <iostream> // temporary debugging

#include "net_ip/endpoints_resolver.hpp"
#include "net_ip/worker.hpp"

using namespace std::experimental::net;

template <typename Protocol>
void make_endpoints_test (bool local, std::string_view host, std::string_view port, bool expected_good) {

  using results_t = ip::basic_resolver_results<Protocol>;
  using prom_ret = std::pair<std::error_code, results_t>;

  using namespace std::literals::chrono_literals;

  chops::net::worker wk;
  wk.start();

  chops::net::endpoints_resolver<Protocol> resolver(wk.get_io_context());

  GIVEN ("An executor work guard, host, and port strings") {
    WHEN ("sync overload of make_endpoints is called") {
      THEN ("a sequence of endpoints is returned or an exception thrown") {
        if (expected_good) {
          auto results = resolver.make_endpoints(local, host, port);
std::cerr << "Results size: " << results.size() << std::endl;
          for (const auto& i : results) {
            INFO ("-- Endpoint: " << i.endpoint());
std::cerr << "-- Endpoint: " << i.endpoint() << std::endl;
          }
          REQUIRE_FALSE (results.empty());
        }
        else {
          REQUIRE_THROWS (resolver.make_endpoints(local, host, port));
        }
      }
    }
    AND_WHEN ("async overload of make_endpoints is called") {
      THEN ("a sequence of endpoints is returned through a function object callback") {

        std::promise<prom_ret> res_prom;
        auto fut = res_prom.get_future();
        resolver.make_endpoints(
          [p = std::move(res_prom)] (const std::error_code& err, results_t res) mutable {
              p.set_value(prom_ret(err, res));
            }, local, host, port);
        auto a = fut.get();
        if (a.first) {
          INFO ("Error val: " << a.first);
std::cerr << "Error val: " << a.first << std::endl;
        }
        if (expected_good) {
std::cerr << "Results size: " << a.second.size() << std::endl;
          for (const auto& i : a.second) {
            INFO ("-- Endpoint: " << i.endpoint());
std::cerr << "-- Endpoint: " << i.endpoint() << std::endl;
          }
          REQUIRE_FALSE (a.second.empty());
        }
        else {
          REQUIRE (a.second.empty());
        }
      }
    }
  } // end given

  std::this_thread::sleep_for(3s); // sleep for 3 seconds
  wk.reset();

}

SCENARIO ( "Make endpoints remote test, TCP  1", 
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<ip::tcp> (false, "www.cnn.com", "80", true);

}

SCENARIO ( "Make endpoints remote test, TCP 2",
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<ip::tcp> (false, "www.seattletimes.com", "80", true);

}

SCENARIO ( "Make endpoints local test, TCP 3",
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<ip::tcp> (true, "", "23000", true);

}

SCENARIO ( "Make endpoints remote test, UDP  1",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<ip::udp> (false, "www.cnn.com", "80", true);

}

SCENARIO ( "Make endpoints remote test, UDP 2",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<ip::udp> (false, "www.seattletimes.com", "80", true);

}

SCENARIO ( "Make endpoints local test, UDP 3",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<ip::udp> (true, "", "23000", true);

}

/*
SCENARIO ( "Make endpoints remote test, TCP invalid", "[tcp_make_endpoints_invalid]" ) {

  make_endpoints_test<ip::tcp> (false, "frobozz.blaaaarg", "32555", false);

}
*/

