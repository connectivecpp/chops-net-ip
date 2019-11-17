/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Unit tests for classes and functions in tcp_connector_timeout.hpp.
 *
 *  @author Nathan Deutsch
 *
 *  Copyright (c) 2019 by Cliff Green, Nathan Deutsch
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"
#include "net_ip/tcp_connector_timeout.hpp"

using namespace std::chrono_literals;

using opt_ms = std::optional<std::chrono::milliseconds>;
const opt_ms empty_val { opt_ms{ } };


template <typename F>
void common_test(const F& func, opt_ms exp_val, std::size_t num) {
  for (std::size_t i = 0u; i < num; ++i) {
    auto ret = func(i+1u);
    REQUIRE (ret);
    REQUIRE (*ret == exp_val);
  }
}

TEST_CASE("tcp connector timeout", "[tcp_connector_timeout]") {

  {
    const opt_ms expected_val { std::chrono::milliseconds{1000} };
    chops::net::simple_timeout to { }; // default constructed, 1000 timeout
    common_test(to, expected_val, 3u);
  }

  {
    const opt_ms expected_val { std::chrono::milliseconds{500} };
    chops::net::simple_timeout to { std::chrono::milliseconds {500} };
    common_test(to, expected_val, 3u);
  }

  {
    const opt_ms expected_val { std::chrono::milliseconds{1500} };
    chops::net::counted_timeout to { std::chrono::milliseconds {1500}, 4u };
    common_test(to, expected_val, 3u);
    auto ret = to(4u);
    REQUIRE (ret);
    REQUIRE (*ret == expected_val);
    ret = to(5u);
    REQUIRE_FALSE (ret);
    REQUIRE (ret == empty_val);
  }



/*
    // timeout with reconnection attempts disabled
    chops::net::tcp_connector_timeout timeout_no_reconnection(
        100ms, 5000ms, false, false
    );

    REQUIRE(timeout_no_reconnection(0) == std::optional<std::chrono::milliseconds> {100});
    auto result = timeout_no_reconnection(1);
    REQUIRE_FALSE(result);

    // timeout with exponential backoff disabled
    chops::net::tcp_connector_timeout timeout_no_backoff(
        100ms, 5000ms, true, false
    );
    REQUIRE(timeout_no_backoff(0) == std::optional<std::chrono::milliseconds> {100});
    REQUIRE(timeout_no_backoff(1) == std::optional<std::chrono::milliseconds> {100});

    // timeout with exponential backoff enabled
    chops::net::tcp_connector_timeout timeout_with_backoff(
        100ms, 1000001ms, true, true
    );
    REQUIRE(timeout_with_backoff(0) == std::optional<std::chrono::milliseconds> {100});
    REQUIRE(timeout_with_backoff(1) == std::optional<std::chrono::milliseconds> {10000});
    REQUIRE(timeout_with_backoff(2) == std::optional<std::chrono::milliseconds> {1000000});
    // check for max_timeout override (100^50 is too big)
    REQUIRE(timeout_with_backoff(3) == std::optional<std::chrono::milliseconds> {1000001});
*/
}
