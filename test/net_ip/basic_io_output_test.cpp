/** @file
 *
 * @brief Test scenarios for @c basic_io_output class template.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2018-2025 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <memory> // std::shared_ptr
#include <set>
#include <cstddef> // std::size_t

#include "net_ip/queue_stats.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"

#include "buffer/shared_buffer.hpp"
#include "utility/byte_array.hpp"

#include "shared_test/mock_classes.hpp"

template <typename IOT>
void basic_io_output_test_construction() {

  chops::net::basic_io_output<IOT> io_out { };
  REQUIRE_FALSE (io_out.is_valid());
  auto ioh = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf(ioh);
  auto ret = io_intf.make_io_output();
  REQUIRE (ret);
  REQUIRE ((*ret).is_valid());

  chops::net::basic_io_output<IOT> io_out1(ioh);
  chops::net::basic_io_output<IOT> io_out2(io_out1);
  REQUIRE(io_out1.is_valid());
  REQUIRE(io_out2.is_valid());
}

template <typename IOT>
void basic_io_output_test_sends() {

  auto ioh = std::make_shared<IOT>();
  chops::net::basic_io_output<IOT> io_out(ioh);
  REQUIRE (io_out.is_valid());
  ioh->start_io();

  auto s = io_out.get_output_queue_stats();
  REQUIRE (s);
  REQUIRE ((*s).output_queue_size == chops::test::io_handler_mock::qs_base);
  REQUIRE ((*s).bytes_in_output_queue == (chops::test::io_handler_mock::qs_base + 1));

  chops::const_shared_buffer buf(nullptr, 0);
  using endp_t = typename IOT::endpoint_type;

  REQUIRE (io_out.is_valid());

  io_out.send(nullptr, 0);
  io_out.send(buf);
  io_out.send(chops::mutable_shared_buffer());
  io_out.send(nullptr, 0, endp_t());
  io_out.send(buf, endp_t());
  io_out.send(chops::mutable_shared_buffer(), endp_t());
  REQUIRE(ioh->send_called);

}

template <typename IOT>
void check_set (const std::set<chops::net::basic_io_output<IOT>>& io_set,
                const chops::net::basic_io_output<IOT>& io1,
                const chops::net::basic_io_output<IOT>& io2,
                const chops::net::basic_io_output<IOT>& io3) {

  // three entries - first empty, next two are valid entries, could be in either order
  REQUIRE (io_set.size() == 3u);
  auto i = io_set.cbegin();
  REQUIRE_FALSE (i->is_valid());
  REQUIRE (*i == io1);
  ++i;
  REQUIRE (i->is_valid());
  REQUIRE ((*i == io2 || *i == io3));
  ++i;
  REQUIRE (i->is_valid());
  REQUIRE ((*i == io2 || *i == io3));
}

template <typename IOT>
void basic_io_output_test_compare() {

  chops::net::basic_io_output<IOT> io_emp1 { };
  chops::net::basic_io_output<IOT> io_emp2(io_emp1);

  auto ioh1 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf1(ioh1);
  auto io_out1 = *(io_intf1.make_io_output());

  auto ioh2 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf2(ioh2);
  auto io_out2 = *(io_intf2.make_io_output());

  auto io_out3 = io_out1;
  auto io_out4 = io_out2;

  REQUIRE (io_emp1 == io_emp2);
  REQUIRE_FALSE (io_out1 == io_out2);
  REQUIRE_FALSE (io_out3 == io_out4);
  REQUIRE (io_out1 == io_out3);
  REQUIRE (io_out2 == io_out4);
  REQUIRE (io_emp1 < io_out1);
  REQUIRE (io_emp2 < io_out1);
  REQUIRE (io_emp1 < io_out2);
  REQUIRE (io_emp2 < io_out2);
  REQUIRE (io_emp1 < io_out3);
  REQUIRE (io_emp2 < io_out4);

  REQUIRE ((io_out1 < io_out2 || io_out2 < io_out1));

  std::set<chops::net::basic_io_output<IOT> > a_set {
          io_out4, io_out3, io_out2, io_out1, io_emp2, io_emp1 };
  check_set(a_set, io_emp1, io_out1, io_out2);
  check_set(a_set, io_emp2, io_out3, io_out4);
}

TEST_CASE ( "Basic io output test, io_handler_mock used for IO handler type",
            "[basic_io_output] [io_handler_mock]" ) {
  basic_io_output_test_construction<chops::test::io_handler_mock>();
  basic_io_output_test_sends<chops::test::io_handler_mock>();
  basic_io_output_test_compare<chops::test::io_handler_mock>();
}

