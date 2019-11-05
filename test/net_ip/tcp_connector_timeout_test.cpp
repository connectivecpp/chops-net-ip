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

 
#include "net_ip/tcp_connector_timeout.hpp"

SCENARIO ( "tcp connector timeout", "[tcp_connector_timeout]" ) {
    using namespace chops::test;

    // timeout with reconnection attempts disabled
    tcp_connector_timeout timeout_no_reconnection = new tcp_connector_timeout(
        100, 5000, false, false
    );
    GIVEN("A tcp_connector_timeout that has reconnection set to \"false\"") {
        WHEN("The operator() method is called with 0 connection attempts") {
            THEN("The initial timeout is returned") {
                REQUIRE(timeout_no_reconnection(0) == std::optional<std::chrono::milliseconds> {100});
            }
        }
        AND_WHEN("The operator() method is called with more than 0 connection attempts") {
            THEN("An empty optional is returned") {
                REQUIRE(timeout_no_reconnection(1) == {});
            }
        }
    }

    // timeout with exponential backoff disabled
    tcp_connector_timeout timeout_no_backoff = new tcp_connector_timeout(
        100, 5000, true, false
    );
    GIVEN("A tcp_connector_timeout that has reconnection set to \"true\" and backoff set to \"false\"") {
        WHEN("The operator() method is called repeatedly") {
            THEN("The initial timeout is returned") {
                REQUIRE(timeout_no_backoff(0) == std::optional<std::chrono::milliseconds> {100});
                REQUIRE(timeout_no_backoff(1) == std::optional<std::chrono::milliseconds> {100});
            }
        }
    }

    // timeout with exponential backoff enabled
    tcp_connector_timeout timeout_with_backoff = new tcp_connector_timeout(
        100, 10001, 100, true, true
    );
    GIVEN("A tcp_connector_timeout that has reconnection set to \"true\" and backoff set to \"true\"") {
        WHEN("The operator() method is called repeatedly") {
            THEN("An exponentially increasing optional is returned") {
                REQUIRE(timeout_with_backoff(0) == std::optional<std::chrono::milliseconds> {100});
                REQUIRE(timeout_with_backoff(1) == std::optional<std::chrono::milliseconds> {1000});
                REQUIRE(timeout_with_backoff(2) == std::optional<std::chrono::milliseconds> {10000});
            }
        }
        AND_WHEN("The calculated exponent exceeds \"max timeout\"") {
            THEN("The \"max_timeout\" is returned") {
                REQUIRE(timeout_with_backoff(50) == std::optional<std::chrono::milliseconds> {20000});
            }
        }
    }
}
