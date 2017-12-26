/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_ip error and exception code.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <system_error>

#include "net_ip/net_ip_error.hpp"

void will_throw() {
  chops::net::net_ip_exception ex(std::make_error_code(chops::net::net_ip_errc::message_handler_terminated));
  throw ex;
}

SCENARIO ( "Error code and exception test", "[error_code_exception]" ) {

  GIVEN ("A function that throws a net_ip exception") {
    WHEN ("The function throws") {
      THEN ("an exception is present") {
        REQUIRE_THROWS (will_throw());
      }
    }
  } // end given

  GIVEN ("A function that throws a net_ip exception") {
    WHEN ("The function throws") {
      THEN ("the exception will contain a custom error message") {
        try {
          will_throw();
        }
        catch (const chops::net::net_ip_exception& e) {
          INFO ("Error code message: " << e.err.message());
          REQUIRE(e.err);
        }
      }
    }
  } // end given

}

