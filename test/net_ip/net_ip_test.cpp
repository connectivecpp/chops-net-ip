/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_ip class.
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
#include "net_ip_component/error_delivery.hpp"
#include "net_ip_component/io_state_change.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/msg_handling_start_funcs.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/repeat.hpp"

#include <iostream> // std::cerr for error sink

using namespace chops::test;

const char* tcp_test_port = "30465";
const char* tcp_test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 400;

const char*   udp_test_addr = "127.0.0.1";
constexpr int udp_port_base = 31445;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread


auto get_tcp_io_futures(chops::net::net_entity ent, chops::net::err_wait_q& err_wq,
                        bool reply, std::string_view delim, test_counter& cnt) {
  return delim.empty() ? 
    chops::net::make_io_output_future_pair<chops::net::tcp_io>(ent,
           chops::net::make_simple_variable_len_msg_frame_io_state_change(2, 
                                                                          tcp_msg_hdlr(reply, cnt),
                                                                          decode_variable_len_msg_hdr),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)) :
    chops::net::make_io_output_future_pair<chops::net::tcp_io>(ent,
           chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
}

auto start_tcp_acceptor(chops::net::net_entity acc, chops::net::err_wait_q& err_wq,
                        bool reply, std::string_view delim, test_counter& cnt) {

  return delim.empty() ?
    acc.start(chops::net::make_simple_variable_len_msg_frame_io_state_change(2,
                                                                             tcp_msg_hdlr(reply, cnt),
                                                                             decode_variable_len_msg_hdr),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)) :
    acc.start(chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
}

auto get_udp_io_future(chops::net::net_entity udp_ent, chops::net::err_wait_q& err_wq,
                       bool reply, test_counter& cnt) {
  return chops::net::make_io_output_future<chops::net::udp_io>(udp_ent,
           chops::net::make_read_io_state_change(udp_max_buf_size, udp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));
}

auto get_udp_io_future(chops::net::net_entity udp_ent, chops::net::err_wait_q& err_wq,
                       bool receiving, test_counter& cnt,
                       const asio::ip::udp::endpoint& remote_endp) {

  return receiving ?
    chops::net::make_io_output_future<chops::net::udp_io>(udp_ent,
             chops::net::make_default_endp_io_state_change(remote_endp, 
                                                           udp_max_buf_size, 
                                                           udp_msg_hdlr(false, cnt)),
             chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq)) :
    chops::net::make_io_output_future<chops::net::udp_io>(udp_ent,
           chops::net::make_send_only_default_endp_io_state_change(remote_endp),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));
}

std::size_t acc_conn_var_test (asio::io_context& ioc, chops::net::err_wait_q& err_wq,
                               const vec_buf& var_msg_vec, bool reply, int num_conns,
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

  chops::repeat(num_conns, [&nip, &err_wq, delim, &conn_cnt, &send_vec, &conn_fut_vec] () {

      auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host, 0);
      auto conn_futs = get_tcp_io_futures(conn, err_wq,
                                          false, delim, conn_cnt);

      send_vec.emplace_back(conn_futs.start_fut.get()); // block until connector connects
      conn_fut_vec.emplace_back(std::move(conn_futs.stop_fut)); // add disconnect future

    }
  );

  for (const auto& buf : var_msg_vec) {
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

std::size_t acc_conn_fixed_test (asio::io_context& ioc, chops::net::err_wait_q& err_wq,
                                 const vec_buf& fixed_msg_vec, int num_conns) {

  chops::net::net_ip nip(ioc);

  std::promise<std::size_t> prom;
  auto acc_start_fut = prom.get_future();
  auto acc = nip.make_tcp_acceptor(tcp_test_port, tcp_test_host);
  acc.start([num_conns, &prom] (chops::net::tcp_io_interface io_intf, std::size_t num, bool starting) {
        if (starting) {
          auto r = io_intf.start_io(); // send only through acceptor
          assert(r);
          if (num == num_conns) {
            prom.set_value(num);
          }
        }
      },
    chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
  );
  auto acc_st = acc.is_started();
  REQUIRE (acc_st);
  REQUIRE (*acc_st);

  INFO("Acceptor created, now creating connectors and futures, num: " << num_conns);

  test_counter conn_cnt = 0;
  std::vector<std::future<std::size_t>> conn_futs;

  chops::repeat(num_conns, [&nip, &conn_futs, &conn_cnt, exp = fixed_msg_vec.size(), &err_wq] () {

      auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host, ReconnTime);
      auto prom_ptr = std::make_shared<test_prom>();
      conn_futs.push_back(std::move(prom_ptr->get_future()));
      auto r = conn.start( [&conn_cnt, exp, &err_wq, prom_ptr]
                       (chops::net::tcp_io_interface io, std::size_t num, bool starting) {
            if (starting) {
              auto r = io.start_io(fixed_size_buf_size,
                                   tcp_fixed_size_msg_hdlr(std::move(*prom_ptr), exp, conn_cnt));
              assert(r);
            }
          },
        chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
      );
      assert (r);
    }
  );

  auto n = acc_start_fut.get(); // means all connectors have connected
  REQUIRE (n == num_conns);

  for (const auto& buf : fixed_msg_vec) {
    auto n = acc.visit_io_output([&buf] (chops::net::tcp_io_output io) {
        io.send(buf);
      }
    );
    assert (n == num_conns);
  }

  for (auto& fut : conn_futs) { // wait for all connectors to finish receiving data
    auto t = fut.get();
  }

  acc.stop();
  nip.remove(acc);
  INFO ("Acceptor stopped and removed");

  nip.stop_all(); // stop all of the connectors
  nip.remove_all();
  INFO ("Connectors stopped and removed");

  return conn_cnt;
}

std::size_t udp_test (asio::io_context& ioc, chops::net::err_wait_q& err_wq, 
                      const vec_buf& msg_vec, int interval, int num_udp_pairs) {

  chops::net::net_ip nip(ioc);

  INFO ("Creating " << num_udp_pairs << " udp sender receiver pairs");

  test_counter recv_cnt = 0;
  test_counter send_cnt = 0;

  std::vector<chops::net::net_entity> senders;

  chops::repeat(num_udp_pairs, [&senders, &err_wq, &recv_cnt, &send_cnt, &nip] (int i) {
      auto recv_endp = make_udp_endpoint(udp_test_addr, udp_port_base + i);

      auto udp_receiver = nip.make_udp_unicast(recv_endp);
      auto recv_fut = get_udp_io_future(udp_receiver, err_wq,
                                        false, recv_cnt );
      auto udp_sender = nip.make_udp_sender();
      senders.push_back(udp_sender);

      auto sender_fut = get_udp_io_future(udp_sender, err_wq,
                                          false, send_cnt, recv_endp );
      recv_fut.get(); // block until receiver ready
      sender_fut.get(); // block until sender ready
    }
  );

  // send messages through all of the senders
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  for (const auto& buf : msg_vec) {
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

  return recv_cnt;
}

void perform_test (const vec_buf& var_msg_vec, const vec_buf& fixed_msg_vec,
                   bool reply, int interval, 
                   int num_entities, std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, chops::net::ostream_error_sink_with_wait_queue,
                            std::ref(err_wq), std::ref(std::cerr));

  {
    std::size_t total_msgs = num_entities * var_msg_vec.size();
    auto cnt1 = acc_conn_var_test(ioc, err_wq, var_msg_vec, reply, num_entities, delim, empty_msg);
    REQUIRE (cnt1 == total_msgs);
    auto cnt2 = udp_test(ioc, err_wq, var_msg_vec, interval, num_entities);
    CHECK (cnt2 == total_msgs);
  }

  {
    std::size_t total_msgs = num_entities * fixed_msg_vec.size();
    auto cnt1 = acc_conn_fixed_test(ioc, err_wq, fixed_msg_vec, num_entities);
    REQUIRE (cnt1 == total_msgs);
    auto cnt2 = udp_test(ioc, err_wq, fixed_msg_vec, interval, num_entities);
    CHECK (cnt2 == total_msgs);
  }

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

  perform_test(make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
               make_fixed_size_msg_vec(NumMsgs),
               false, 50, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, one-way, interval 0, 1 connector or pair", 
            "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
               make_fixed_size_msg_vec(2*NumMsgs),
               false, 0, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 50, 1 connector or pair", 
            "[net_ip] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
               make_fixed_size_msg_vec(NumMsgs),
               true, 50, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
               make_fixed_size_msg_vec(100*NumMsgs),
               true, 0, 10,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 40 connectors or pairs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs),
               make_fixed_size_msg_vec(100*NumMsgs),
               true, 0, 40,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs),
               make_fixed_size_msg_vec(NumMsgs),
               false, 50, 1,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
               make_fixed_size_msg_vec(NumMsgs),
               false, 50, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 0, 20 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
               make_fixed_size_msg_vec(4*NumMsgs),
               false, 0, 20,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 30, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
               make_fixed_size_msg_vec(4*NumMsgs),
               true, 30, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
               make_fixed_size_msg_vec(200*NumMsgs),
               true, 0, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
               make_fixed_size_msg_vec(NumMsgs),
               false, 50, 1,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 0, 25 connectors or pairs", 
            "[net_ip] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
               make_fixed_size_msg_vec(6*NumMsgs),
               false, 0, 25,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 20, 15 connectors or pairs", 
            "[net_ip] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs),
               make_fixed_size_msg_vec(2*NumMsgs),
               true, 20, 15,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 0, 15 connectors or pairs, many msgs", 
            "[net_ip] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs),
               make_fixed_size_msg_vec(300*NumMsgs),
               true, 0, 15,
               std::string_view("\n"), make_empty_lf_text_msg() );

}
