/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for @c send_to_all class template.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch.hpp"

#include <cstddef> // std::size_t

#include <memory> // std::make_shared

#include "net_ip/component/send_to_all.hpp"

#include "net_ip/queue_stats.hpp"
#include "utility/shared_buffer.hpp"

#include "net_ip/shared_utility_test.hpp"

SCENARIO ( "Testing send_to_all class",
           "[send_to_all]" ) {

  using namespace chops::test;

  chops::net::send_to_all<io_handler_mock> sta { };
  REQUIRE (sta.size() == 0);

  GIVEN ("A default constructed send_to_all object") {
    WHEN ("add_io_interface is called") {
      auto ioh = std::make_shared<io_handler_mock>();
      sta.add_io_interface(io_interface_mock(ioh));
      THEN ("the size increases by 1") {
        REQUIRE (sta.size() == 1);
      }
    }
    AND_WHEN ("remove_io_interface or function call operator is called") {
      auto ioh1 = std::make_shared<io_handler_mock>();
      auto ioh2 = std::make_shared<io_handler_mock>();
      auto ioh3 = std::make_shared<io_handler_mock>();
      sta.add_io_interface(io_interface_mock(ioh1));
      sta(io_interface_mock(ioh2), 1, true);
      sta.add_io_interface(io_interface_mock(ioh3));
      REQUIRE (sta.size() == 3);
      THEN ("the size decreases by 1 for each call") {
        sta.remove_io_interface(io_interface_mock(ioh2));
        REQUIRE (sta.size() == 2);
        sta(io_interface_mock(ioh1), 0, false);
        REQUIRE (sta.size() == 1);
        sta.remove_io_interface(io_interface_mock(ioh3));
        REQUIRE (sta.size() == 0);
      }
    }
    AND_WHEN ("send is called") {
      std::byte b(static_cast<std::byte>(0xFE));
      chops::const_shared_buffer buf(&b, 1);
      auto ioh1 = std::make_shared<io_handler_mock>();
      REQUIRE_FALSE(ioh1->send_called);
      auto ioh2 = std::make_shared<io_handler_mock>();
      REQUIRE_FALSE(ioh2->send_called);
      sta.add_io_interface(io_interface_mock(ioh1));
      sta.add_io_interface(io_interface_mock(ioh2));
      REQUIRE (sta.size() == 2);
      sta.send(buf);
      THEN ("the send flag is true") {
        REQUIRE(ioh1->send_called);
        REQUIRE(ioh2->send_called);
      }
    }
    AND_WHEN ("get_total_output_queue_stats is called") {
      auto ioh1 = std::make_shared<io_handler_mock>();
      auto ioh2 = std::make_shared<io_handler_mock>();
      sta.add_io_interface(io_interface_mock(ioh1));
      sta.add_io_interface(io_interface_mock(ioh2));
      THEN ("the right results are returned") {
        auto tot = sta.get_total_output_queue_stats();
        REQUIRE(tot.output_queue_size == sta.size() * io_handler_mock::qs_base);
        REQUIRE(tot.bytes_in_output_queue == sta.size() * (io_handler_mock::qs_base + 1));
      }
    }
  } // end given
}



