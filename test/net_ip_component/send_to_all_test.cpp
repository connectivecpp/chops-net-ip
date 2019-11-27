/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for @c send_to_all class template.
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

#include <cstddef> // std::size_t

#include <memory> // std::make_shared

#include "net_ip_component/send_to_all.hpp"

#include "net_ip/queue_stats.hpp"
#include "marshall/shared_buffer.hpp"

#include "shared_test/mock_classes.hpp"

TEST_CASE ( "Testing send_to_all class", "[send_to_all]" ) {

  using namespace chops::test;

  chops::net::send_to_all<io_handler_mock> sta { };
  REQUIRE (sta.size() == 0u);

  auto ioh1 = std::make_shared<io_handler_mock>();
  auto ioh2 = std::make_shared<io_handler_mock>();
  auto ioh3 = std::make_shared<io_handler_mock>();
  auto ioh4 = std::make_shared<io_handler_mock>();

  io_interface_mock io_intf1(ioh1);
  io_interface_mock io_intf2(ioh2);
  io_interface_mock io_intf3(ioh3);
  io_interface_mock io_intf4(ioh4);

  io_output_mock out1 = *(io_intf1.make_io_output());
  io_output_mock out2 = *(io_intf2.make_io_output());
  io_output_mock out3 = *(io_intf3.make_io_output());
  io_output_mock out4 = *(io_intf4.make_io_output());

  sta.add_io_output(out1);
  REQUIRE (sta.size() == 1u);
  sta.add_io_output(out2);
  sta(io_intf3, 1u, true);
  sta(io_intf4, 1u, true);
  REQUIRE (sta.size() == 4u);

  sta.remove_io_output(out2);
  REQUIRE (sta.size() == 3u);
  sta(io_intf1, 0u, false);
  REQUIRE (sta.size() == 2u);
  sta.remove_io_output(out3);
  REQUIRE (sta.size() == 1u);
  sta(io_interface_mock(ioh4), 0, false);
  REQUIRE (sta.size() == 0u);

  std::byte b(static_cast<std::byte>(0xFE));
  chops::const_shared_buffer buf(&b, 1u);
  REQUIRE_FALSE(ioh1->send_called);
  REQUIRE_FALSE(ioh2->send_called);
  sta.add_io_output(out1);
  sta.add_io_output(out2);
  REQUIRE (sta.size() == 2u);
  sta.send(buf);
  REQUIRE(ioh1->send_called);
  REQUIRE(ioh2->send_called);

  auto tot = sta.get_total_output_queue_stats();
  REQUIRE(tot.output_queue_size == sta.size() * io_handler_mock::qs_base);
  REQUIRE(tot.bytes_in_output_queue == sta.size() * (io_handler_mock::qs_base + 1));
}
