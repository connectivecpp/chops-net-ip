/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c io_interface delivery functions.
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

#include <chrono>
#include <thread>
#include <memory>
#include <future>

#include "net_ip_component/io_interface_delivery.hpp"
#include "net_ip_component/io_state_change.hpp"

#include "net_ip/basic_net_entity.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

#include "net_ip/shared_utility_test.hpp"


SCENARIO ( "Testing make_io_interface_future_impl",
           "[io_interface_delivery]" ) {

  using namespace chops::test;
  using basic_net_mock = chops::net::basic_net_entity<net_entity_mock>;

  GIVEN ("A entity object") {
    auto ent_ptr = std::make_shared<net_entity_mock>();
    auto ent = basic_net_mock (ent_ptr);
    WHEN ("make_io_interface_future_impl is called") {
      auto fut = 
        chops::net::detail::make_io_interface_future_impl<io_handler_mock>(ent,
                                                                           io_state_chg_mock,
                                                                           err_func_mock);

      THEN ("a future is returned, which returns a value when ready") {
          auto io = fut.get();
          REQUIRE (ent.is_started());
          ent.stop();
          REQUIRE_FALSE (ent.is_started());
      }
    }
    AND_WHEN ("make_io_interface_future_pair_impl is called") {
      auto pair_fut = 
        chops::net::detail::make_io_interface_future_pair_impl<io_handler_mock>(ent,
                                                                           io_state_chg_mock,
                                                                           err_func_mock);

      THEN ("two futures are returned, both of which return a value when ready") {
          auto io1 = pair_fut.start_fut.get();
          auto io2 = pair_fut.stop_fut.get();
          ent.stop();
      }
    }
  } // end given
}

SCENARIO ( "Testing start_with_wait_queue",
           "[io_interface_delivery]" ) {

  using namespace chops::test;
  using basic_net_mock = chops::net::basic_net_entity<net_entity_mock>;

  GIVEN ("An entity object and a wait_queue") {
    auto ent_ptr = std::make_shared<net_entity_mock>();
    auto ent = basic_net_mock (ent_ptr);
    chops::net::io_wait_q<io_handler_mock> wq;
    WHEN ("start_with_wait_queue is called") {
      chops::net::start_with_wait_queue<io_handler_mock>(ent, io_state_chg_mock, 
                                                         wq, err_func_mock);
      THEN ("io interface objects are delivered through the wait_queue") {
          auto elem1 = wq.wait_and_pop();
          REQUIRE (elem1);
          REQUIRE ((*elem1).starting);
          REQUIRE ((*elem1).num_handlers == 1);
          auto elem2 = wq.wait_and_pop();
          REQUIRE (elem2);
          REQUIRE_FALSE ((*elem2).starting);
          REQUIRE ((*elem2).num_handlers == 0);
          ent.stop();
      }
    }
  } // end given
}


