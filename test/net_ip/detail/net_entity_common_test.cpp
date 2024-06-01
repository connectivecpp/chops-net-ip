/** @file
 *
 * @brief Test scenarios for @c net_entity_common detail class.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2018-2024 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <memory> // std::shared_ptr
#include <system_error> // std::error_code
#include <utility> // std::move
#include <functional> // std::ref
#include <cstddef> // std::size_t

#include "net_ip/detail/net_entity_common.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

#include "shared_test/mock_classes.hpp"

#include "net_ip_component/worker.hpp"

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

std::error_code start_stop () { return std::error_code { }; }

template <typename IOT>
void net_entity_common_test() {

  using namespace chops::net;

  io_state_change<IOT> io_state_chg;
  REQUIRE_FALSE (io_state_chg.called);
  REQUIRE_FALSE (io_state_chg.ioh_valid);

  err_callback<IOT> err_cb;
  REQUIRE_FALSE (err_cb.called);
  REQUIRE_FALSE (err_cb.ioh_valid);

  {
    // test set_stopped
    detail::net_entity_common<IOT> ne { };
    REQUIRE_FALSE (ne.is_started());
    REQUIRE_FALSE (ne.is_stopped());
    ne.set_stopped();
    REQUIRE_FALSE (ne.is_started());
    REQUIRE (ne.is_stopped());
  }

  detail::net_entity_common<IOT> ne { };
  REQUIRE_FALSE (ne.is_started());
  REQUIRE_FALSE (ne.is_stopped());

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  // test stop before being started
  auto e = ne.stop(ioc.get_executor(), start_stop);
  REQUIRE (e);
  INFO (e.message());

  auto iohp = std::make_shared<IOT>();
  auto r = ne.start(std::ref(io_state_chg), std::ref(err_cb), ioc.get_executor(), start_stop);
  REQUIRE_FALSE (r);
  REQUIRE (ne.is_started());

  ne.call_io_state_chg_cb(iohp, 43, true);
  ne.call_error_cb(iohp, std::make_error_code(net_ip_errc::io_already_started));

  REQUIRE (io_state_chg.called);
  REQUIRE (io_state_chg.ioh_valid);
  REQUIRE (io_state_chg.num == 43);

  REQUIRE (err_cb.called);
  REQUIRE (err_cb.ioh_valid);
  REQUIRE (err_cb.err);

  e = ne.stop(ioc.get_executor(), start_stop);
  REQUIRE_FALSE (e);
  REQUIRE_FALSE (ne.is_started());
  REQUIRE (ne.is_stopped());

  r = ne.start(std::ref(io_state_chg), std::ref(err_cb), ioc.get_executor(), start_stop);
  REQUIRE (r);
  INFO (r.message());
  REQUIRE_FALSE (ne.is_started());

  wk.reset();

}

TEST_CASE ( "Net entity base test", "[net_entity_common]" ) {
  net_entity_common_test<chops::test::io_handler_mock>();
}

