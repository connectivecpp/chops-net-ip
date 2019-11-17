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
void common_fixed_to_test(const F& func, opt_ms exp_val, std::size_t num) {
  for (std::size_t i = 0u; i < num; ++i) {
    auto ret = func(i+1u);
    REQUIRE (ret);
    REQUIRE (*ret == exp_val);
  }
}

template <typename F>
void common_progressive_to_test(const F& func, opt_ms exp_val1, opt_ms exp_val2,
                                               opt_ms exp_val3, opt_ms exp_val4) {
  auto ret = func(1u);
  REQUIRE (ret);
  REQUIRE (*ret == exp_val1);
  ret = func(2u);
  REQUIRE (ret);
  REQUIRE (*ret == exp_val2);
  ret = func(3u);
  REQUIRE (ret);
  REQUIRE (*ret == exp_val3);
  ret = func(4u);
  REQUIRE (ret);
  REQUIRE (*ret == exp_val4);
}

TEST_CASE("tcp connector timeout", "[tcp_connector_timeout]") {

  {
    const opt_ms expected_val { 1000ms };
    chops::net::simple_timeout to { }; // default constructed, 1000 timeout

    common_fixed_to_test(to, expected_val, 3u);
  }

  {
    const opt_ms expected_val { 500ms };
    chops::net::simple_timeout to { 500ms };

    common_fixed_to_test(to, expected_val, 3u);
  }

  {
    const opt_ms expected_val { 1500ms };
    chops::net::counted_timeout to { 1500ms, 4u };

    common_fixed_to_test(to, expected_val, 4u);
    auto ret = to(5u);
    REQUIRE_FALSE (ret);
    REQUIRE (ret == empty_val);
  }

  {
    const opt_ms expected_val1 { 400ms };
    const opt_ms expected_val2 { 800ms };
    const opt_ms expected_val3 { 1600ms };
    const opt_ms expected_val4 { 2000ms };

    chops::net::backoff_timeout to { 400ms, 2000ms };

    common_progressive_to_test(to, expected_val1, expected_val2,
                                   expected_val3, expected_val4);

  }

  {
    const opt_ms expected_val1 { 300ms };
    const opt_ms expected_val2 { 900ms };
    const opt_ms expected_val3 { 1800ms };
    const opt_ms expected_val4 { 2677ms };
    chops::net::backoff_timeout to { 300ms, 2677ms, 3 };

    common_progressive_to_test(to, expected_val1, expected_val2,
                                   expected_val3, expected_val4);

  }

  INFO("Testing exponential backoff, 100ms initial, 100,000ms max");

  {
    const opt_ms expected_val1 { 100ms };
    const opt_ms expected_val2 { 10000ms };
    const opt_ms expected_val3 { 1000000ms };
    const opt_ms expected_val4 { 1000001ms };
    chops::net::exponential_backoff_timeout to { 100ms, 1000001ms };

    common_progressive_to_test(to, expected_val1, expected_val2,
                                   expected_val3, expected_val4);

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
