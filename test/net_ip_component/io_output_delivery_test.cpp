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
#include "net_ip/net_ip.hpp"
#include "net_ip/io_type_decls.hpp"


const char* test_port_acc = "30222";
const char* test_port_conn = "30223";
const char* test_port_udp = "30224";
const char* test_host = "";

template <typename IOT>
struct null_io_state_chg {
  bool operator()(chops::net::basic_io_interface<IOT>, std::size_t, bool) const { return true; }
};

template <typename IOT>
void test_io_wait_q(chops::net::net_entity net_ent, chops::net::err_wait_q& err_wq,
                    int exp_entries) {

    auto r = net_ent.is_started();
    REQUIRE (r);
    REQUIRE_FALSE (*r);
    chops::net::io_wait_q<IOT> wq;
    auto t = chops::net::start_with_io_wait_queue<IOT>(net_ent, null_io_state_chg<IOT>(), wq, 
                                   chops::net::make_error_func_with_wait_queue<IOT>(err_wq));
    REQUIRE (t);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    net_ent.stop();
    while (exp_entries > 0) {
      auto r = wq.wait_and_pop();
      REQUIRE (r);
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

  chops::net::net_ip nip(ioc);

  {
    {
      auto acc_ent = nip.make_tcp_acceptor(test_port_acc);
      REQUIRE (acc_ent.is_valid());
      test_io_wait_q<chops::net::tcp_io>(acc_ent, err_wq, 0);

      auto conn1_ent = nip.make_tcp_connector(test_port_conn, test_host);
      REQUIRE (conn1_ent.is_valid());
      test_io_wait_q<chops::net::tcp_io>(conn1_ent, err_wq, 0);

      auto udp_ent = nip.make_udp_sender();
      REQUIRE (udp_ent.is_valid());
      test_io_wait_q<chops::net::udp_io>(udp_ent, err_wq, 2);

      acc_ent.stop();
      conn1_ent.stop();
      udp_ent.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

//
      auto fut = chops::net::make_io_output_future<chops::net::udp_io>(udp_ent, 
                        null_io_state_chg<chops::net::udp_io>(),
                        chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));

      auto r1 = udp_ent.is_started();
      REQUIRE (r1);
      REQUIRE (*r1);
      auto io1 = fut.get();
      udp_ent.stop();
      auto r2 = udp_ent.is_started();
      REQUIRE (r2);
      REQUIRE_FALSE (*r2);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

//
      auto pair_fut = chops::net::make_io_output_future_pair<chops::net::udp_io>(udp_ent, 
                        null_io_state_chg<chops::net::udp_io>(),
                        chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));

      auto r3 = udp_ent.is_started();
      REQUIRE (r3);
      REQUIRE (*r3);
      auto io2 = pair_fut.start_fut.get();
      udp_ent.stop();
      auto r4 = udp_ent.is_started();
      REQUIRE (r4);
      REQUIRE_FALSE (*r4);
      auto io3 = pair_fut.stop_fut.get();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

//
      auto t2 = acc_ent.start(null_io_state_chg<chops::net::tcp_io>(),
                             chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
      REQUIRE (t2);

      auto conn2_ent = nip.make_tcp_connector(test_port_acc, test_host);
      REQUIRE (conn2_ent.is_valid());

      auto conn_pair_fut = chops::net::make_io_output_future_pair<chops::net::tcp_io>(conn2_ent, 
                        null_io_state_chg<chops::net::tcp_io>(),
                        chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
      auto r5 = conn2_ent.is_started();
      REQUIRE (r5);
      REQUIRE (*r5);
      auto io5 = conn_pair_fut.start_fut.get();
      conn2_ent.stop();
      acc_ent.stop();
      auto io6 = conn_pair_fut.stop_fut.get();

      auto r6 = conn2_ent.is_started();
      REQUIRE (r6);
      REQUIRE_FALSE (*r6);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }

    while (!err_wq.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    err_wq.close();
    auto err_cnt = err_fut.get();
    INFO ("Num err messages in sink: " << err_cnt);

  }

  nip.remove_all();

  wk.reset();
}


