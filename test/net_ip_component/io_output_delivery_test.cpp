/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c basic_io_output delivery functions.
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
#include <iostream> // std::cerr, used as error sink

#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/error_delivery.hpp"
#include "net_ip_component/worker.hpp"

#include "net_ip/net_entity.hpp"
#include "net_ip/io_type_decls.hpp"


const char* test_port_acc = "30222";
const char* test_port_conn = "30223";
const char* test_port_udp = "30224";
const char* test_host = "";

template <typename IOT>
struct io_state_chg {
  bool operator()(chops::net::basic_io_interface<IOT>, std::size_t, bool) const { return true; }
};

template <typename IOT>
void test_io_wait_q(chops::net::net_entity net_ent, chops::net::err_wait_q& err_wq,
                    int exp_entries) {

    auto r = net_ent.is_started();
    REQUIRE (r);
    REQUIRE_FALSE (*r);
    chops::net::io_wait_q<IOT> wq;
    auto t = chops::net::start_with_io_wait_queue<IOT>(net_ent, io_state_chg<IOT>(), wq, 
                                   chops::net::make_error_func_with_wait_queue<IOT>(err_wq));
    REQUIRE (t);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    net_ent.stop();
    while (exp_entries > 0) {
      auto r = wq.wait_and_pop();
      REQUIRE (r);
      r->io_out.release();
      --exp_entries;
    }
}

SCENARIO ( "Testing make_io_output_future and start_with_io_wait_queue",
           "[io_output_delivery]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, 
  chops::net::ostream_error_sink_with_wait_queue, std::ref(err_wq), std::ref(std::cerr));

  {
    auto sp_acc = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                             std::string_view(test_port_acc), std::string_view(test_host), true);
    chops::net::net_entity ne_acc(sp_acc);
    REQUIRE (ne_acc.is_valid());
    test_io_wait_q<chops::net::tcp_io>(ne_acc, err_wq, 0);

    auto sp_conn1 = std::make_shared<chops::net::detail::tcp_connector>(ioc, 
                             std::string_view(test_port_conn), std::string_view(test_host), 
                             std::chrono::milliseconds(500));
    chops::net::net_entity ne_conn1(sp_conn1);
    REQUIRE (ne_conn1.is_valid());
    test_io_wait_q<chops::net::tcp_io>(ne_conn1, err_wq, 0);

    auto sp_udp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, 
                             std::string_view(test_port_udp), std::string_view(test_host));
    chops::net::net_entity ne_udp(sp_udp);



    REQUIRE (ne_udp.is_valid());
    test_io_wait_q<chops::net::udp_io>(ne_udp, err_wq, 2);

//

    auto fut = chops::net::make_io_output_future<chops::net::udp_io>(ne_udp, 
                      io_state_chg<chops::net::udp_io>(),
                      chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));

    auto r1 = ne_udp.is_started();
    REQUIRE (r1);
    REQUIRE (*r1);
    auto io1 = fut.get();
    io1.release();
    ne_udp.stop();
    auto r2 = ne_udp.is_started();
    REQUIRE (r2);
    REQUIRE_FALSE (*r2);

    auto pair_fut = chops::net::make_io_output_future_pair<chops::net::udp_io>(ne_udp, 
                      io_state_chg<chops::net::udp_io>(),
                      chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));


    auto r3 = ne_udp.is_started();
    REQUIRE (r3);
    REQUIRE (*r3);
    auto io2 = pair_fut.start_fut.get();
    io2.release();
    ne_udp.stop();
    auto r4 = ne_udp.is_started();
    REQUIRE (r4);
    REQUIRE_FALSE (*r4);
    auto io3 = pair_fut.stop_fut.get();
    io3.release();

    auto t2 = ne_acc.start(io_state_chg<chops::net::tcp_io>(),
                                   chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
    REQUIRE (t2);

    auto sp_conn2 = std::make_shared<chops::net::detail::tcp_connector>(ioc, 
                             std::string_view(test_port_acc), std::string_view(test_host), 
                             std::chrono::milliseconds(500));
    chops::net::net_entity ne_conn2(sp_conn2);
    REQUIRE (ne_conn2.is_valid());

    auto conn_pair_fut = chops::net::make_io_output_future_pair<chops::net::tcp_io>(ne_conn2, 
                      io_state_chg<chops::net::tcp_io>(),
                      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
    auto r5 = ne_conn2.is_started();
    REQUIRE (r5);
    REQUIRE (*r5);
    auto io5 = conn_pair_fut.start_fut.get();
    io5.release();
    ne_conn2.stop();
    ne_acc.stop();
    auto io6 = conn_pair_fut.stop_fut.get();
    io6.release();

    auto r6 = ne_conn2.is_started();
    REQUIRE (r6);
    REQUIRE_FALSE (*r6);

    while (!err_wq.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    err_wq.close();
    auto err_cnt = err_fut.get();
    INFO ("Num err messages in sink: " << err_cnt);

  }

  wk.reset();
}


