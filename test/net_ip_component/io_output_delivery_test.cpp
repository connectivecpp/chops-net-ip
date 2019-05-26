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
#include <thread>
#include <chrono>
#include <string_view>
#include <cstddef>

#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/worker.hpp"

#include "net_ip/net_entity.hpp"
#include "net_ip/io_type_decls.hpp"

///
#include <iostream>

const char* test_port_acc = "30222";
const char* test_port_conn = "30223";
const char* test_port_udp = "30224";
const char* test_host = "";

template <typename IOT>
bool io_state_chg (chops::net::basic_io_interface<IOT>, std::size_t, bool) { return true; }

template <typename IOT>
void test_io_wait_q(chops::net::net_entity net_ent, int exp_entries) {

    auto r = net_ent.is_started();
    REQUIRE (r);
    REQUIRE_FALSE (*r);
    chops::net::io_wait_q<IOT> wq;
    auto t = chops::net::start_with_io_wait_queue<IOT>(net_ent, io_state_chg<IOT>, wq, 
                                              chops::net::empty_error_func<IOT>);
//    REQUIRE (t);
std::cerr << "Error return: " << t.error().message() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
/*
    net_ent.stop();
    while (exp_entries > 0) {
      auto r = wq.wait_and_pop();
      REQUIRE (r);
      --exp_entries;
    }
*/
}

SCENARIO ( "Testing make_io_output_future and start_with_io_wait_queue",
           "[io_output_delivery]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  {
    auto sp_acc = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                             std::string_view(test_port_acc), std::string_view(test_host), true);
    chops::net::net_entity ne_acc(sp_acc);
    REQUIRE (ne_acc.is_valid());
    test_io_wait_q<chops::net::tcp_io>(ne_acc, 0);

    auto sp_conn = std::make_shared<chops::net::detail::tcp_connector>(ioc, 
                             std::string_view(test_port_conn), std::string_view(test_host), 
                             std::chrono::milliseconds(500));
    chops::net::net_entity ne_conn(sp_conn);
    REQUIRE (ne_conn.is_valid());
    test_io_wait_q<chops::net::tcp_io>(ne_conn, 0);

    auto sp_udp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, 
                             std::string_view(test_port_udp), std::string_view(test_host));
    chops::net::net_entity ne_udp(sp_udp);
    REQUIRE (ne_udp.is_valid());
    test_io_wait_q<chops::net::udp_io>(ne_udp, 2);

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

  }

  wk.reset();
}


