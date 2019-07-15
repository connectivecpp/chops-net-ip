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
  REQUIRE_FALSE (io_intf.is_valid());

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

template <typename IOT, typename F>
void basic_io_interface_test_start_io(F&& func) {

  auto ioh = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf(ioh);
  REQUIRE (io_intf.is_valid());
  REQUIRE_FALSE (*io_intf.is_io_started());

  auto r = func(io_intf);
  REQUIRE (r);
  REQUIRE (*io_intf.is_io_started());
  auto t = io_intf.stop_io();
  REQUIRE (t);
  REQUIRE_FALSE (*io_intf.is_io_started());

}

template <typename IOT>
void basic_io_interface_test_all_start_io() {

  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { return io.start_io(0, [] { }, [] { }); }
  );
  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { return io.start_io(0, [] { }, do_nothing_hdr_decoder); }
  );
  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { return io.start_io("testing, hah!", [] { }); }
  );
  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { return io.start_io(0, [] { }); }
  );
  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { 
      using endp_t = typename IOT::endpoint_type;
      return io.start_io(endp_t(), 0, [] { });
    }
  );
  basic_io_interface_test_start_io<IOT>([] (chops::net::basic_io_interface<IOT> io) 
    { 
      using endp_t = typename IOT::endpoint_type;
      return io.start_io(endp_t());
    }
  );
}



template <typename IOT>
void basic_io_interface_test_other_methods() {

  auto ioh = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf(ioh);
  REQUIRE (io_intf.is_valid());

  auto io_out = io_intf.make_io_output();
  REQUIRE (io_out);
  REQUIRE ((*io_out).is_valid());

  auto r = io_intf.visit_socket([] (double& d) { d += 1.0; } );
  REQUIRE (r);
  REQUIRE (ioh->mock_sock == 43.0);

}

template <typename IOT>
void check_set (const std::set<chops::net::basic_io_interface<IOT>>& io_set,
                const chops::net::basic_io_interface<IOT>& io1,
                const chops::net::basic_io_interface<IOT>& io2,
                const chops::net::basic_io_interface<IOT>& io3) {
  // three entries, first is empty, second and third could be in either order
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
void basic_io_interface_test_compare() {

  chops::net::basic_io_interface<IOT> io_intf1 { };

  auto ioh1 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf2(ioh1);

  chops::net::basic_io_interface<IOT> io_intf3 { };

  auto ioh2 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf4(ioh2);

  chops::net::basic_io_interface<IOT> io_intf5 { };

  REQUIRE (io_intf1 == io_intf3);
  REQUIRE (io_intf3 == io_intf5);
  REQUIRE_FALSE (io_intf2 == io_intf4);

  // 1, 3, 5 are empty, 2 and 4 are valid, but different
  std::set<chops::net::basic_io_interface<IOT> > a_set1 
      { io_intf5, io_intf4, io_intf3, io_intf2, io_intf1 };
  check_set(a_set1, io_intf1, io_intf2, io_intf4);

  io_intf2 = io_intf4;
  REQUIRE (io_intf2 == io_intf4);
  REQUIRE (io_intf1 < io_intf2);
  REQUIRE (io_intf3 < io_intf4);

  auto ioh3 = std::make_shared<IOT>();
  chops::net::basic_io_interface<IOT> io_intf6(ioh3);
  // 1, 3, 5 empty, 2 and 4 are valid and the same, 6 is valid
  std::set<chops::net::basic_io_interface<IOT> > a_set2 
      { io_intf6, io_intf5, io_intf4, io_intf3, io_intf2, io_intf1 };
  check_set(a_set2, io_intf3, io_intf4, io_intf6);

  chops::net::basic_io_interface<IOT> io_intfa(io_intf1);
  chops::net::basic_io_interface<IOT> io_intfb(io_intf1);
  chops::net::basic_io_interface<IOT> io_intfc(io_intf1);
  chops::net::basic_io_interface<IOT> io_intfd(io_intf2);
  chops::net::basic_io_interface<IOT> io_intfe(io_intf2);
  chops::net::basic_io_interface<IOT> io_intff(io_intf4);
  chops::net::basic_io_interface<IOT> io_intfg(io_intf4);
  chops::net::basic_io_interface<IOT> io_intfh(io_intf4);
  chops::net::basic_io_interface<IOT> io_intfi(io_intf4);
  chops::net::basic_io_interface<IOT> io_intfj(io_intf5);
  chops::net::basic_io_interface<IOT> io_intfk(io_intf6);
      
  std::set<chops::net::basic_io_interface<IOT> > a_set3
      { io_intfa, io_intfb, io_intfc, io_intfd, io_intfe, io_intff, 
        io_intfg, io_intfh, io_intfi, io_intfj, io_intfk };

  check_set(a_set3, io_intf5, io_intf2, io_intf6);

}

TEST_CASE ( "Basic io interface test, io_handler_mock used for IO handler type",
           "[basic_io_interface] [io_handler_mock]" ) {
  basic_io_interface_test_default_constructed<chops::test::io_handler_mock>();
  basic_io_interface_test_all_start_io<chops::test::io_handler_mock>();
  basic_io_interface_test_other_methods<chops::test::io_handler_mock>();
  basic_io_interface_test_compare<chops::test::io_handler_mock>();
}

