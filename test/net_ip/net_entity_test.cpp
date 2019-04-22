/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr
#include <functional>
#include <system_error> // std::error_code
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/net_entity.hpp"

#include "net_ip/shared_utility_test.hpp"

void net_entity_test_default_constructed() {

  chops::net::net_entity net_ent { };

  GIVEN ("A default constructed net_entity") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (net_ent.is_valid());
      }
    }
    AND_WHEN ("methods are called on an invalid net_entity") {
      THEN ("an exception is thrown") {
        REQUIRE_THROWS (net_ent.is_started());
        REQUIRE_THROWS (net_ent.start(chops::test::io_state_chg_mock, chops::test::err_func_mock));
        REQUIRE_THROWS (net_ent.stop());
      }
    }
  } // end given

}

template <typename EH>
void net_entity_test_methods() {

  chops::net::net_entity net_ent { };

  auto e = std::make_shared<EH>();
  net_ent = chops::net::net_entity(e); // want a weak ptr to the shared ptr

  GIVEN ("A default constructed net_entity and an io handler") {
    WHEN ("a net_entity with a weak ptr to the io handler is assigned to it") {
      THEN ("is_valid is true") {
        REQUIRE (net_ent.is_valid());
      }
    }
    AND_WHEN ("is_started is called") {
      THEN ("false is returned") {
        REQUIRE_FALSE (net_ent.is_started());
      }
    }
    AND_WHEN ("start or stop is called") {
      THEN ("true is returned") {
        REQUIRE (net_ent.start(chops::test::io_state_chg_mock, chops::test::err_func_mock));
        REQUIRE (net_ent.is_started());
        REQUIRE (net_ent.stop());
        REQUIRE_FALSE (net_ent.is_started());
      }
    }
    AND_WHEN ("get_socket is called") {
      THEN ("a reference is returned") {
        REQUIRE (net_ent.get_socket() == chops::test::net_entity_mock::special_val);
      }
    }
  } // end given

}

template <typename EH>
void net_entity_test_compare() {

  chops::net::net_entity net_ent1 { };
  auto e1 = std::make_shared<EH>();
  chops::net::net_entity net_ent2(e1);
  chops::net::net_entity net_ent3 { };
  auto e2 = std::make_shared<EH>();
  chops::net::net_entity net_ent4(e2);
  chops::net::net_entity net_ent5 { };

  GIVEN ("Three default constructed net_entities and two with entities") {
    WHEN ("all three are inserted in a multiset") {
      std::multiset<chops::net::net_entity> a_set { net_ent1, net_ent2, net_ent3, net_ent4, net_ent5 };
      THEN ("the invalid net_entitie are first in the set") {
        REQUIRE (a_set.size() == 5);
        auto i = a_set.cbegin();
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
      }
    }
    AND_WHEN ("two invalid net_entities are compared for equality") {
      THEN ("they compare equal") {
        REQUIRE (net_ent1 == net_ent3);
        REQUIRE (net_ent3 == net_ent5);
      }
    }
    AND_WHEN ("two valid net_entities are compared for equality") {
      THEN ("they compare equal if both point to the same io handler") {
        REQUIRE_FALSE (net_ent2 == net_ent4);
        net_ent2 = net_ent4;
        REQUIRE (net_ent2 == net_ent4);
      }
    }
    AND_WHEN ("an invalid net_entity is order compared with a valid net_entity") {
      THEN ("the invalid compares less than the valid") {
        REQUIRE (net_ent1 < net_ent2);
      }
    }
  } // end given

}

SCENARIO ( "Basic net entity test", "[net_entity]" ) {
  net_entity_test_default_constructed<chops::test::net_entity_mock>();
  net_entity_test_methods<chops::test::net_entity_mock>();
  net_entity_test_compare<chops::test::net_entity_mock>();
}

