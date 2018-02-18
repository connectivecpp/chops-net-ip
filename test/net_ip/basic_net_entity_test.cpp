/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_net_entity class template.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet> // endpoint declarations

#include <memory> // std::shared_ptr
#include <functional>
#include <system_error> // std::error_code
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/basic_net_entity.hpp"

#include "net_ip/shared_utility_test.hpp"

template <typename EH>
void basic_net_entity_test_default_constructed() {

  chops::net::basic_net_entity<EH> net_ent { };

  GIVEN ("A default constructed basic_net_entity") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (net_ent.is_valid());
      }
    }
    AND_WHEN ("is_started on an invalid basic_net_entity") {
      THEN ("an exception is thrown") {
        REQUIRE_THROWS (net_ent.is_started());
      }
    }
    AND_WHEN ("start or stop is called on an invalid basic_net_entity") {
      THEN ("false is returned") {

        REQUIRE_FALSE (net_ent.start([] { }, [] { } ));
        REQUIRE_FALSE (net_ent.stop());
      }
    }
  } // end given

}

template <typename EH>
void basic_net_entity_test_two() {

  chops::net::basic_net_entity<EH> net_ent { };

  auto e = std::make_shared<EH>();
  net_ent = chops::net::basic_net_entity<EH>(e); // want a weak ptr to the shared ptr

  GIVEN ("A default constructed basic_net_entity and an io handler") {
    WHEN ("a basic_net_entity with a weak ptr to the io handler is assigned to it") {
      THEN ("the return is true") {
        REQUIRE (net_ent.is_valid());
      }
    }
    AND_WHEN ("is_started or get_output_queue_stats is called") {
      THEN ("values are returned") {
        REQUIRE_FALSE (net_ent.is_started());
      }
    }
    AND_WHEN ("start or stop is called") {
      THEN ("true is returned") {
        REQUIRE (net_ent.start([] { }, [] { }));
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
void basic_net_entity_test_compare() {

  chops::net::basic_net_entity<EH> net_ent1 { };
  auto e1 = std::make_shared<EH>();
  chops::net::basic_net_entity<EH> net_ent2(e1);
  chops::net::basic_net_entity<EH> net_ent3 { };
  auto e2 = std::make_shared<EH>();
  chops::net::basic_net_entity<EH> net_ent4(e2);
  chops::net::basic_net_entity<EH> net_ent5 { };

  GIVEN ("Three default constructed net_entities and two with entities") {
    WHEN ("all three are inserted in a multiset") {
      std::multiset<chops::net::basic_net_entity<EH> > a_set { net_ent1, net_ent2, net_ent3, net_ent4, net_ent5 };
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
    AND_WHEN ("an invalid basic_net_entity is order compared with a valid basic_net_entity") {
      THEN ("the invalid compares less than the valid") {
        REQUIRE (net_ent1 < net_ent2);
      }
    }
  } // end given

}

SCENARIO ( "Basic net entity test", "[basic_net_entity]" ) {
  basic_net_entity_test_default_constructed<chops::test::net_entity_mock>();
  basic_net_entity_test_two<chops::test::net_entity_mock>();
  basic_net_entity_test_compare<chops::test::net_entity_mock>();
}

