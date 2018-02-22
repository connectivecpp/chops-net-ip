/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_io_interface class template.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <memory> // std::shared_ptr
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/queue_stats.hpp"
#include "net_ip/basic_io_interface.hpp"

#include "net_ip/shared_utility_test.hpp"

#include "utility/shared_buffer.hpp"
#include "utility/make_byte_array.hpp"

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
      THEN ("an exception is thrown") {

        chops::const_shared_buffer buf(nullptr, 0);
        using endp_t = typename IOT::endpoint_type;

        REQUIRE_THROWS (io_intf.is_io_started());
        REQUIRE_THROWS (io_intf.get_socket());
        REQUIRE_THROWS (io_intf.get_output_queue_stats());

        REQUIRE_THROWS (io_intf.send(nullptr, 0));
        REQUIRE_THROWS (io_intf.send(buf));
        REQUIRE_THROWS (io_intf.send(chops::mutable_shared_buffer()));
        REQUIRE_THROWS (io_intf.send(nullptr, 0, endp_t()));
        REQUIRE_THROWS (io_intf.send(buf, endp_t()));
        REQUIRE_THROWS (io_intf.send(chops::mutable_shared_buffer(), endp_t()));

        REQUIRE_THROWS (io_intf.start_io(0, [] { }, [] { }));
        REQUIRE_THROWS (io_intf.start_io("testing, hah!", [] { }));
        REQUIRE_THROWS (io_intf.start_io(0, [] { }));
        REQUIRE_THROWS (io_intf.start_io(endp_t(), 0, [] { }));
        REQUIRE_THROWS (io_intf.start_io());
        REQUIRE_THROWS (io_intf.start_io(endp_t()));

        REQUIRE_THROWS (io_intf.stop_io());
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
    AND_WHEN ("is_io_started or get_output_queue_stats is called") {
      THEN ("correct values are returned") {
        REQUIRE_FALSE (io_intf.is_io_started());
        chops::net::output_queue_stats s = io_intf.get_output_queue_stats();
        REQUIRE (s.output_queue_size == chops::test::io_handler_mock::qs_base);
        REQUIRE (s.bytes_in_output_queue == (chops::test::io_handler_mock::qs_base + 1));
      }
    }
    AND_WHEN ("send or start_io or stop_io is called") {
      THEN ("appropriate values are set or returned") {

        chops::const_shared_buffer buf(nullptr, 0);
        using endp_t = typename IOT::endpoint_type;

        REQUIRE (io_intf.is_valid());

        io_intf.send(nullptr, 0);
        io_intf.send(buf);
        io_intf.send(chops::mutable_shared_buffer());
        io_intf.send(nullptr, 0, endp_t());
        io_intf.send(buf, endp_t());
        io_intf.send(chops::mutable_shared_buffer(), endp_t());
        REQUIRE(ioh->send_called);

        REQUIRE (io_intf.start_io(0, [] { }, [] { }));
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE_FALSE (io_intf.is_io_started());
        REQUIRE (io_intf.start_io("testing, hah!", [] { }));
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE (io_intf.start_io(0, [] { }));
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE (io_intf.start_io(endp_t(), 0, [] { }));
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE (io_intf.start_io(endp_t()));
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE (io_intf.start_io());
        REQUIRE (io_intf.is_io_started());
        REQUIRE (io_intf.stop_io());
        REQUIRE_FALSE (io_intf.is_io_started());
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

