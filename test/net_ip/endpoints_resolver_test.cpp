/** @file
 *
 * @brief Test scenarios for @c make_endpoints functions.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2018-2025 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ip/basic_resolver.hpp"

#include <system_error> // std::error_code
#include <utility> // std::pair
#include <future>
#include <string_view>

#include "net_ip/endpoints_resolver.hpp"
#include "net_ip_component/worker.hpp"

template <typename Protocol>
void make_endpoints_test (bool local, std::string_view host, std::string_view port, bool expected_good) {

  using results_t = asio::ip::basic_resolver_results<Protocol>;
  using prom_ret = std::pair<std::error_code, results_t>;

  using namespace std::literals::chrono_literals;

  chops::net::worker wk;
  wk.start();

  chops::net::endpoints_resolver<Protocol> resolver(wk.get_io_context());

  GIVEN ("An executor work guard, host, and port strings") {
    WHEN ("sync overload of make_endpoints is called") {
      THEN ("a sequence of endpoints is returned or an error returned") {
        INFO ("-- Host: " << host);
        auto results = resolver.make_endpoints(local, host, port);
        if (expected_good) {
          REQUIRE(results);
          INFO ("-- Num endpoints: " << results->size());
          REQUIRE_FALSE (results->empty());
        }
        else {
          REQUIRE_FALSE(results);
          INFO ("Error code: " << results.error());
        }
      }
    }
    AND_WHEN ("async overload of make_endpoints is called") {
      THEN ("a sequence of endpoints is returned through a function object callback") {

        INFO ("-- Host: " << host);
        std::promise<prom_ret> res_prom;
        auto fut = res_prom.get_future();
        resolver.make_endpoints(local, host, port,
          [p = std::move(res_prom)] (const std::error_code& err, results_t res) mutable {
            p.set_value(prom_ret(err, res));
          }
        );
        auto a = fut.get();
        if (expected_good) {
          REQUIRE_FALSE(a.first);
          INFO ("-- Num endpoints: " << a.second.size());
          REQUIRE_FALSE (a.second.empty());
        }
        else {
          REQUIRE(a.first);
          INFO ("Error val: " << a.first);
          REQUIRE (a.second.empty());
        }
      }
    }
  } // end given

  wk.reset();

}

SCENARIO ( "Make endpoints remote test, TCP  1", 
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<asio::ip::tcp> (false, "www.cnn.com", "80", true);

}

SCENARIO ( "Make endpoints remote test, TCP 2",
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<asio::ip::tcp> (false, "www.seattletimes.com", "80", true);

}

SCENARIO ( "Make endpoints local test, TCP 3",
           "[make_endpoints] [tcp]" ) {

  make_endpoints_test<asio::ip::tcp> (true, "", "23000", true);

}

SCENARIO ( "Make endpoints remote test, UDP  1",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<asio::ip::udp> (false, "www.cnn.com", "80", true);

}

SCENARIO ( "Make endpoints remote test, UDP 2",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<asio::ip::udp> (false, "www.seattletimes.com", "80", true);

}

SCENARIO ( "Make endpoints local test, UDP 3",
           "[make_endpoints] [udp]" ) {

  make_endpoints_test<asio::ip::udp> (true, "", "23000", true);

}

/*
SCENARIO ( "Make endpoints remote test, TCP invalid", "[tcp_make_endpoints_invalid]" ) {

  make_endpoints_test<asio::ip::tcp> (false, "frobozz.blaaaarg", "32555", false);

}
*/

