/** @file
 *
 * @brief Test scenarios for @c net_ip error and exception code.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2017-2025 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <system_error>

#include "net_ip/net_ip_error.hpp"

void will_throw() {
  chops::net::net_ip_exception ex(std::make_error_code(chops::net::net_ip_errc::message_handler_terminated));
  throw ex;
}

TEST_CASE ( "Error code and exception test", "[error_code_exception]" ) {

  REQUIRE_THROWS (will_throw());

  try {
    will_throw();
  }
  catch (const chops::net::net_ip_exception& e) {
    INFO ("Error code message: " << e.err.message());
    REQUIRE(e.err);
  }
}

