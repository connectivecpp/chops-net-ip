/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c erase_where utility functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <vector>

#include "utility/erase_where.hpp"

SCENARIO( "Richard Hodge's erase_where combines erase with remove", "[erase_where]" ) {

  GIVEN ("A vector of ints") {
    std::vector<int> vec { 0, 1, 2, 3, 4, 5, 6, 7 };

    WHEN ("erase_where is called with a value") {
      chops::erase_where(vec, 5);
      THEN ("the vector size is one less and the value is removed") {
        REQUIRE (vec.size() == 7);
        REQUIRE (vec == (std::vector<int> { 0, 1, 2, 3, 4, 6, 7 }) );
      }
    }
    AND_WHEN ("erase_where is called with a predicate removing anything < 3") {
      chops::erase_where_if(vec, [] (int i) { return i < 3; } );
      THEN ("multiple values are removed") {
        REQUIRE (vec.size() == 5);
        REQUIRE (vec == (std::vector<int> { 3, 4, 5, 6, 7 }) );
      }
    }
  } // end given
}
