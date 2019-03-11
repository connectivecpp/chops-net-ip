/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity_common detail class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr
#include <system_error> // std::error_code
#include <utility> // std::move
#include <functional> // std::ref
#include <cstddef> // std::size_t

#include "net_ip/detail/net_entity_common.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

#include "net_ip/shared_utility_test.hpp"

template<typename IOT>
struct io_state_change {

  bool called = false;
  std::size_t num = 0;
  bool ioh_valid = false;

  void operator() (chops::net::basic_io_interface<IOT> ioh, std::size_t n, bool starting) {
    called = true;
    num = n;
    ioh_valid = ioh.is_valid();
  }
};

template<typename IOT>
struct err_callback {

  bool called = false;
  bool ioh_valid = false;
  std::error_code err;

  void operator() (chops::net::basic_io_interface<IOT> ioh, std::error_code e) {
    called = true;
    err = e;
    ioh_valid = ioh.is_valid();
  }
};

template <typename IOT>
void net_entity_common_test() {

  using namespace chops::net;

  io_state_change<IOT> io_state_chg;
  REQUIRE_FALSE (io_state_chg.called);
  REQUIRE_FALSE (io_state_chg.ioh_valid);

  err_callback<IOT> err_cb;
  REQUIRE_FALSE (err_cb.called);
  REQUIRE_FALSE (err_cb.ioh_valid);

  detail::net_entity_common<IOT> ne { };
  REQUIRE_FALSE (ne.is_started());

  auto iohp = std::make_shared<IOT>();

  GIVEN ("A default constructed net_entity_common and a state change object") {

    WHEN ("Start is called") {
      ne.start(std::ref(io_state_chg), std::ref(err_cb));
      THEN ("net entity base is started") {
        REQUIRE (ne.is_started());
      }
    }

    AND_WHEN ("Stop is called") {
      ne.start(std::ref(io_state_chg), std::ref(err_cb));
      ne.stop();
      THEN ("net entity base is not started") {
        REQUIRE_FALSE (ne.is_started());
      }
    }

    AND_WHEN ("State change and error callbacks are invoked") {
      ne.start(std::ref(io_state_chg), std::ref(err_cb));
      ne.call_io_state_chg_cb(iohp, 43, true);
      ne.call_error_cb(iohp, std::make_error_code(net_ip_errc::tcp_io_handler_stopped));
      THEN ("function object vals are set correctly") {
        REQUIRE (io_state_chg.called);
        REQUIRE (io_state_chg.ioh_valid);
        REQUIRE (io_state_chg.num == 43);

        REQUIRE (err_cb.called);
        REQUIRE (err_cb.ioh_valid);
        REQUIRE (err_cb.err);
      }
    }

  } // end given
}

SCENARIO ( "Net entity base test", "[net_entity_common]" ) {
  net_entity_common_test<chops::test::io_handler_mock>();
}

