/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Unit tests for tcp_connector_timeout
 *
 *  @author Nathan Deutsch
 *
 *  Copyright (c) 2019 by Cliff Green and Nathan Deutsch
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"
#include "net_ip/tcp_connector_timeout.hpp"

using namespace std::chrono_literals;

TEST_CASE("tcp connector timeout", "[tcp_connector_timeout]") {
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
}
