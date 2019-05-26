/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_io_output delivery functions.
 *
 *  Very little runtime processing and testing is performed in these unit tests. It
 *  is primarily used for compilation and API testing purposes.
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

#include <future>
#include <memory>
#include <string_view>
#include <cstddef>

#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/worker.hpp"

#include "net_ip/net_entity.hpp"
#include "net_ip/io_type_decls.hpp"

const char* test_port = "30222";
const char* test_host = "";

template <typename IOT>
bool io_state_chg (chops::net::basic_io_interface<IOT>, std::size_t, bool) { return true; }

SCENARIO ( "Testing make_io_output_future and start_with_io_wait_queue",
           "[io_output_delivery]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  {
    auto ent_sp = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                             std::string_view(test_port), std::string_view(test_host), true);
    chops::net::net_entity net_ent(ent_sp);

    chops::net::tcp_io_wait_q wq;

    REQUIRE (net_ent.is_valid());

/*
    auto fut = chops::net::make_io_output_future<chops::net::tcp_io>(net_ent, 
                                io_state_chg<chops::net::tcp_io>, chops::net::tcp_empty_error_func);

    auto r1 = net_ent.is_started();
    REQUIRE (r1);
    REQUIRE (*r1);
    auto io1 = fut.get();
    net_ent.stop();
    auto r2 = net_ent.is_started();
    REQUIRE (r2);
    REQUIRE_FALSE (*r2);

    auto pair_fut = chops::net::make_io_output_future_pair<chops::net::tcp_io>(net_ent, 
                      io_state_chg<chops::net::tcp_io>, chops::net::tcp_empty_error_func);


    auto r3 = net_ent.is_started();
    REQUIRE (r3);
    REQUIRE (*r3);
    auto io2 = pair_fut.start_fut.get();
    net_ent.stop();
    auto r4 = net_ent.is_started();
    REQUIRE (r4);
    REQUIRE_FALSE (*r4);
    auto io3 = pair_fut.stop_fut.get();

*/
    chops::net::start_with_io_wait_queue<chops::net::tcp_io>(net_ent, 
            io_state_chg<chops::net::tcp_io>, wq, chops::net::tcp_empty_error_func);
    net_ent.stop();
    auto p1 = wq.wait_and_pop();
    REQUIRE (p1);
    REQUIRE (p1->num_handlers == 1u);
    REQUIRE (p1->starting);
    auto p2 = wq.wait_and_pop();
    REQUIRE (p2);
    REQUIRE(p2->num_handlers == 0u);
    REQUIRE_FALSE(p2->starting);

  }

  wk.reset();
}


