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

bool udp_empty_io_state_func (chops::net::udp_io_interface, std::size_t, bool) { return true; }

struct socket_visitor {
  bool func_called = false;
  void operator() (asio::ip::udp::socket& sock) { func_called = true; }
  void operator() (asio::ip::tcp::socket& sock) { func_called = true; }
  void operator() (asio::ip::tcp::acceptor& sock) { func_called = true; }
};

struct io_output_visitor {
  bool func_called = false;
  void operator() (chops::net::tcp_io_output io) { func_called = true; }
  void operator() (chops::net::udp_io_output io) { func_called = true; }
};

SCENARIO ( "Net entity default construction", "[net_entity]" ) {

  chops::net::net_entity net_ent { };

  socket_visitor sv { };
  io_output_visitor io_out { };

  GIVEN ("A default constructed net_entity") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (net_ent.is_valid());
      }
    }
    AND_WHEN ("methods are called on an invalid net_entity") {
      THEN ("an exception is thrown") {
        REQUIRE_THROWS (net_ent.is_started());
        REQUIRE_THROWS (net_ent.visit_socket(sv));
        REQUIRE_THROWS (net_ent.visit_io_output(io_out));
        REQUIRE_THROWS (net_ent.start(udp_empty_io_state_func, chops::net::udp_empty_error_func));
        REQUIRE_THROWS (net_ent.stop());
      }
    }
  } // end given

}

SCENARIO ( "Net entity method testing", "[net_entity]" ) {

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

SCENARIO ( "Net entity comparison testing", "[net_entity]" ) {

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



