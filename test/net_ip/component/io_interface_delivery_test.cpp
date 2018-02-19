/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c io_interface delivery functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <chrono>
#include <thread>
#include <memory>
#include <future>

#include "net_ip/component/io_interface_delivery.hpp"
#include "net_ip/component/io_state_change.hpp"

#include "net_ip/basic_net_entity.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

#include "net_ip/shared_utility_test.hpp"


SCENARIO ( "Testing make_io_interface_future_impl",
           "[io_interface_future]" ) {

  GIVEN ("A entity object") {
    auto ent_ptr = std::make_shared<entity_mock>();
    auto ent = net_entity_mock (ent_ptr);
    WHEN ("make_io_interface_future_impl is called") {
      auto fut = 
        chops::net::detail::make_io_interface_future_impl<io_mock>(ent);

      THEN ("a future is returned, which returns a value when ready") {
          auto io = fut.get();
          REQUIRE (io.get_socket() == 42);
          ent_ptr->join_thr();
      }
    }
    AND_WHEN ("make_io_interface_future_pair_impl is called") {
      auto pair_fut = 
        chops::net::detail::make_io_interface_future_pair_impl<io_mock>(ent);

      THEN ("two futures are returned, both of which return a value when ready") {
          auto io1 = pair_fut.start_fut.get();
          REQUIRE (io1.get_socket() == 42);
          auto io2 = pair_fut.stop_fut.get();
          REQUIRE (io2.get_socket() == 43);
          ent_ptr->join_thr();
      }
    }
  } // end given
}

