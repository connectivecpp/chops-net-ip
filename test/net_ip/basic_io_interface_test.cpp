/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_io_interface class template.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"

#include "shared_test/mock_classes.hpp"

std::size_t do_nothing_hdr_decoder (const std::byte*, std::size_t) { return 0u; }

template <typename IOT>
void basic_io_interface_test_default_constructed() {

  chops::net::basic_io_interface<IOT> io_intf { };

  GIVEN ("A default constructed basic_io_interface") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (io_intf.is_valid());
      }
    }
    AND_WHEN ("any method but comparison is called on an invalid basic_io_interface") {
      THEN ("an error is returned") {

        using endp_t = typename IOT::endpoint_type;

        auto r = io_intf.make_io_output();
        REQUIRE_FALSE (r);
        INFO ("Error: " << r.error().message());

        REQUIRE_FALSE (io_intf.is_io_started());

        REQUIRE_FALSE (io_intf.visit_socket([] (double&) { } ));

        REQUIRE_FALSE (io_intf.start_io(0, [] { }, [] { }));
        REQUIRE_FALSE (io_intf.start_io(0, [] { }, do_nothing_hdr_decoder));
        REQUIRE_FALSE (io_intf.start_io("testing, hah!", [] { }));
        REQUIRE_FALSE (io_intf.start_io(0, [] { }));
        REQUIRE_FALSE (io_intf.start_io(endp_t(), 0, [] { }));
        REQUIRE_FALSE (io_intf.start_io());
        REQUIRE_FALSE (io_intf.start_io(endp_t()));

        REQUIRE_FALSE (io_intf.stop_io());

      }
    }
  } // end given

}

template <typename IOT>
void basic_io_interface_test_methods() {

  chops::net::basic_io_interface<IOT> io_intf { };

  auto ioh = std::make_shared<IOT>();
  io_intf = chops::net::basic_io_interface<IOT>(ioh);

  GIVEN ("A default constructed basic_io_interface and an io handler") {
    WHEN ("a basic_io_interface with a weak ptr to the io handler is assigned to it") {
      THEN ("is_valid returns true") {
        REQUIRE (io_intf.is_valid());
      }
    }
    AND_WHEN ("is_io_started is called") {
      THEN ("false is returned") {
        REQUIRE_FALSE (*io_intf.is_io_started());
      }
    }
    AND_WHEN ("start_io or stop_io is called") {
      THEN ("appropriate values are set or returned") {

        using endp_t = typename IOT::endpoint_type;

        REQUIRE (io_intf.is_valid());

        REQUIRE (*io_intf.start_io(0, [] { }, [] { }));
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE_FALSE (*io_intf.is_io_started());
        REQUIRE (*io_intf.start_io("testing, hah!", [] { }));
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE (*io_intf.start_io(0, [] { }));
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE (*io_intf.start_io(endp_t(), 0, [] { }));
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE (*io_intf.start_io(endp_t()));
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE (*io_intf.start_io());
        REQUIRE (*io_intf.is_io_started());
        REQUIRE (*io_intf.stop_io());
        REQUIRE_FALSE (*io_intf.is_io_started());
      }
    }
    AND_WHEN ("make_io_output is called") {
      THEN ("a basic_io_output object is returned") {
        REQUIRE (io_intf.is_valid());
        auto io_out = io_intf.make_io_output();
        REQUIRE (io_out);
        REQUIRE ((*io_out).is_valid());
      }
    }
    AND_WHEN ("visit_socket is called") {
      THEN ("the io handler value is modified") {
        REQUIRE (io_intf.is_valid());
        auto r = io_intf.visit_socket([] (double& d) { d += 1.0; } );
        REQUIRE (r);
        REQUIRE (ioh->mock_sock == 43.0);
      }
    }

  } // end given

}

template <typename IOT>
void basic_io_interface_test_compare() {

  chops::net::basic_io_interface<IOT> io_intf1 { };

  auto ioh1 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf2(ioh1);

  chops::net::basic_io_interface<IOT> io_intf3 { };

  auto ioh2 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf4(ioh2);

  chops::net::basic_io_interface<IOT> io_intf5 { };

  GIVEN ("Three default constructed basic_io_interfaces and two with io handlers") {
    WHEN ("all three are inserted in a multiset") {
      std::multiset<chops::net::basic_io_interface<IOT> > a_set { io_intf1, io_intf2, io_intf3, io_intf4, io_intf5 };
      THEN ("the invalid basic_io_interfaces are first in the set") {
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
    AND_WHEN ("two invalid basic_io_interfaces are compared for equality") {
      THEN ("they compare equal") {
        REQUIRE (io_intf1 == io_intf3);
        REQUIRE (io_intf3 == io_intf5);
      }
    }
    AND_WHEN ("two valid basic_io_interfaces are compared for equality") {
      THEN ("they compare equal if both point to the same io handler") {
        REQUIRE_FALSE (io_intf2 == io_intf4);
        io_intf2 = io_intf4;
        REQUIRE (io_intf2 == io_intf4);
      }
    }
    AND_WHEN ("an invalid basic_io_interface is order compared with a valid basic_io_interface") {
      THEN ("the invalid compares less than the valid") {
        REQUIRE (io_intf1 < io_intf2);
      }
    }
  } // end given

}

SCENARIO ( "Basic io interface test, io_handler_mock used for IO handler type",
           "[basic_io_interface] [io_handler_mock]" ) {
  basic_io_interface_test_default_constructed<chops::test::io_handler_mock>();
  basic_io_interface_test_methods<chops::test::io_handler_mock>();
  basic_io_interface_test_compare<chops::test::io_handler_mock>();
}

