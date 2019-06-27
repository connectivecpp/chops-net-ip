/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_ip class.
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

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/output_queue_stats.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/start_funcs.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/repeat.hpp"

#include <iostream> // std::cerr for error sink

using namespace chops::test;

const char* tcp_test_port = "30465";
const char* tcp_test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 500;

const char*   udp_test_addr = "127.0.0.1";
constexpr int udp_port_base = 31445;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

std::size_t acc_conn_test (asio::io_context& ioc, chops::net::err_wait_q& err_wq,
                           const vec_buf& in_msg_vec, bool reply, int num_conns,
                           std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::net_ip nip(ioc);
  auto acc = nip.make_tcp_acceptor(tcp_test_port, tcp_test_host);
  REQUIRE (acc.is_valid());

  test_counter acc_cnt = 0;
  auto st = start_tcp_acceptor(acc, err_wq, reply, delim, acc_cnt);
  REQUIRE (st);

  auto r = acc.is_started();
  REQUIRE(r);
  REQUIRE(*r);

  std::vector< chops::net::tcp_io_output > send_vec;
  std::vector< chops::net::tcp_io_output_future > conn_fut_vec;

  test_counter conn_cnt = 0;
  INFO("Acceptor created, now creating connectors and futures, num: " << num_conns);

  chops::repeat(num_conns, [&] () {

      auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host, ReconnTime);
      auto conn_futs = get_tcp_io_futures(conn, err_wq,
                                          false, delim, conn_cnt);

      send_vec.emplace_back(conn_futs.start_fut.get()); // block until connector connects
      conn_fut_vec.emplace_back(std::move(conn_futs.stop_fut)); // add disconnect future

    }
  );

  for (const auto& buf : in_msg_vec) {
    for (auto io : send_vec) {
      io.send(buf);
    }
  }
  for (auto io : send_vec) {
    io.send(empty_msg);
  }

  for (auto& fut : conn_fut_vec) {
    auto io = fut.get(); // block for all disconnects
  }

  acc.stop();
  nip.remove(acc);
  INFO ("Acceptor stopped and removed");

  nip.stop_all(); // stop all of the connectors
  nip.remove_all();
  INFO ("Connectors stopped and removed");

  return acc_cnt;
}

std::size_t udp_test (asio::io_context& ioc, chops::net::err_wait_q& err_wq, 
                      const vec_buf& in_msg_vec, int interval, int num_udp_pairs, 
                      chops::const_shared_buffer empty_msg) {

  chops::net::net_ip nip(ioc);

  INFO ("Creating " << num_udp_pairs << " udp sender receiver pairs");

  test_counter recv_cnt = 0;
  test_counter send_cnt = 0;

  std::vector<chops::net::net_entity> senders;

  chops::repeat(num_udp_pairs, [&] (int i) {
      auto recv_endp = make_udp_endpoint(udp_test_addr, udp_port_base + i);

      auto udp_receiver = nip.make_udp_unicast(recv_endp);
      auto recv_futs = get_udp_io_futures(udp_receiver, err_wq,
                                            false, recv_cnt );
      auto udp_sender = nip.make_udp_sender();
      senders.push_back(udp_sender);

      auto sender_futs = get_udp_io_futures(udp_sender, err_wq,
                                            false, send_cnt, recv_endp );
      recv_futs.start_fut.get(); // block until receiver ready
      sender_futs.start_fut.get(); // block until sender ready
    }
  );

  // send messages through all of the senders
  std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  for (const auto& buf : in_msg_vec) {
    for (auto s : senders) {
      auto r = s.visit_io_output([&buf] (chops::net::udp_io_output io) { io.send(buf); } );
      assert (r);
      assert (*r == 1u);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  // poll output queue size of all senders until 0
  chops::net::accumulate_net_entity_output_queue_stats_until<chops::net::udp_io>
         (senders.cbegin(), senders.cend(), poll_output_queue_cond(200, std::cerr));
                                                               
  std::this_thread::sleep_for(std::chrono::seconds(1));

  nip.stop_all();
  nip.remove_all();
  INFO("All UDP entities stopped and removed");

  std::size_t total_msgs = num_udp_pairs * in_msg_vec.size();
  // CHECK instead of REQUIRE since UDP is an unreliable protocol
  CHECK (total_msgs == recv_cnt);
  return recv_cnt;
}

void perform_test (const vec_buf& in_msg_vec, bool reply, int interval, 
                   int num_entities, std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, chops::net::ostream_error_sink_with_wait_queue,
                            std::ref(err_wq), std::ref(std::cerr));

  std::size_t total_msgs = num_entities * in_msg_vec.size();
  auto cnt1 = acc_conn_test(ioc, err_wq, in_msg_vec, reply, num_entities, delim, empty_msg);
  REQUIRE (cnt1 == total_msgs);
  auto cnt2 = udp_test(ioc, err_wq, in_msg_vec, interval, num_entities, empty_msg);
  CHECK (cnt2 == total_msgs);

  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages in sink: " << err_cnt);

  wk.reset();
}

TEST_CASE ( "Net IP test, var len msgs, one-way, interval 50, 1 connector or pair", 
            "[netip_acc_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  perform_test ( ms, false, 50, 1, std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, one-way, interval 0, 1 connector or pair", 
            "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  perform_test ( ms, false, 0, 1, std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 50, 1 connector or pair", 
            "[net_ip] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  perform_test ( ms, true, 50, 1, std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  perform_test ( ms, true, 0, 10, std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 40 connectors or pairs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs);
  perform_test ( ms, true, 0, 40, std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs);
  perform_test ( ms, false, 50, 1, std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  perform_test ( ms, false, 50, 10, std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 0, 20 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  perform_test ( ms, false, 0, 20, std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 30, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  perform_test ( ms, true, 30, 10, std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  perform_test ( ms, true, 0, 10, std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  perform_test ( ms, false, 50, 1, std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 0, 25 connectors or pairs", 
            "[net_ip] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  perform_test ( ms, false, 0, 25, std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 20, 15 connectors or pairs", 
            "[net_ip] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  perform_test ( ms, true, 20, 15, std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 0, 15 connectors or pairs, many msgs", 
            "[net_ip] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  perform_test ( ms, true, 0, 15, std::string_view("\n"), make_empty_lf_text_msg() );

}
