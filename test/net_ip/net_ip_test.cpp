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
#include <memory> // std::make_shared
#include <utility> // std::move, std::ref
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>


#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip_component/worker.hpp"

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

void acc_conn_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("when done, the counts all match") {

        chops::net::net_ip nip(ioc);

        auto acc = nip.make_tcp_acceptor(tcp_test_port, tcp_test_host);

        chops::net::err_wait_q err_wq;

        auto err_fut = std::async(std::launch::async, 
          chops::net::ostream_error_sink_with_wait_queue,
          std::ref(err_wq), std::ref(std::cerr));

        test_counter acc_cnt = 0;

        start_tcp_acceptor(acc, err_wq, reply, delim, acc_cnt);
        INFO ("Acceptor created");
        REQUIRE(acc.is_valid());
        auto r1 = acc.is_started(); REQUIRE(r1); REQUIRE(*r1);

        std::vector< chops::net::net_entity > connectors;
        std::vector< std::future<chops::net::tcp_io_interface> > conn_fut_vec;

        test_counter conn_cnt = 0;
        INFO("Creating connectors and futures, num: " << num_conns);

        chops::repeat(num_conns, [&] () {

            auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host,
                                               std::chrono::milliseconds(ReconnTime));
            connectors.push_back(conn);

            auto conn_futs = get_tcp_io_futures(conn, err_wq,
                                                false, delim, conn_cnt);

            auto conn_start_io = conn_futs.start_fut.get();
            sta.add_io_interface(conn_start_io);
            conn_fut_vec.emplace_back(std::move(conn_futs.stop_fut));

          }
        );

        for (auto buf : in_msg_vec) {
          sta.send(buf);
        }
        sta.send(empty_msg);

        for (auto& fut : conn_fut_vec) {
          auto io = fut.get();
        }

        acc.stop();
        nip.remove(acc);
        INFO ("Acceptor stopped and removed");

        nip.stop_all();
        nip.remove_all();
        INFO ("Connectors stopped and removed");

        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto err_cnt = err_fut.get();
        INFO ("Num err messages in sink: " << err_cnt);

        std::size_t total_msgs = num_conns * in_msg_vec.size();
        REQUIRE (total_msgs == acc_cnt);
        if (reply) {
          REQUIRE (total_msgs == conn_cnt);
        }
      }
    }
  } // end given
  wk.reset();
}

void udp_test (const vec_buf& in_msg_vec, int interval, int num_udp_pairs, 
               chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("pairs of udp senders and receivers are created") {
      THEN ("when done, the counts all match") {

        chops::net::net_ip nip(ioc);

        INFO ("Creating " << num_udp_pairs << " udp sender receiver pairs");

        chops::net::err_wait_q err_wq;
        auto err_fut = std::async(std::launch::async,
          chops::net::ostream_error_sink_with_wait_queue,
          std::ref(err_wq), std::ref(std::cerr));

        chops::net::send_to_all<chops::net::udp_io> sta { };

        test_counter recv_cnt = 0;
        test_counter send_cnt = 0;

        chops::repeat(num_udp_pairs, [&] (int i) {
            auto recv_endp = make_udp_endpoint(udp_test_addr, udp_port_base + i);

            auto udp_receiver = nip.make_udp_unicast(recv_endp);
            auto recv_futs = get_udp_io_futures(udp_receiver, err_wq,
                                                  false, recv_cnt );
            auto udp_sender = nip.make_udp_sender();

            auto sender_futs = get_udp_io_futures(udp_sender, err_wq,
                                                  false, send_cnt, recv_endp );
            auto send_io = sender_futs.start_fut.get();
            sta.add_io_interface(send_io);
          }
        );

        // send messages through all of the senders
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        for (auto buf : in_msg_vec) {
          sta.send(buf);
          std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        // poll output queue size of all handlers until 0
        auto qs = sta.get_total_output_queue_stats();
std::cerr << "UDP senders total output queue size: " << qs.output_queue_size << std::endl;
        while (qs.output_queue_size > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          qs = sta.get_total_output_queue_stats();
std::cerr << "UDP senders total output queue size: " << qs.output_queue_size << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

        nip.stop_all();
        nip.remove_all();
        INFO("All UDP entities stopped and removed");

        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto err_cnt = err_fut.get();
        INFO ("Num err messages in sink: " << err_cnt);

        std::size_t total_msgs = num_udp_pairs * in_msg_vec.size();
        // CHECK instead of REQUIRE since UDP is an unreliable protocol
        CHECK (total_msgs == recv_cnt);
        CHECK (send_cnt == 0); // no reply
      }
    }
  } // end given
  wk.reset();
}


SCENARIO ( "Net IP test, var len msgs, one-way, interval 50, 1 connector or pair", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 50, 1,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, one-way, interval 0, 1 connector or pair", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  acc_conn_test ( ms, false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 1,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 50, 1 connector or pair", 
           "[net_ip] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  acc_conn_test ( ms, true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 50, 1,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
           "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  acc_conn_test ( ms, true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 10,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 0, 40 connectors or pairs", 
           "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs);
  acc_conn_test ( ms, true, 0, 40, 
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 40, 
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 50, 1 connector or pair", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 50, 1,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 50, 10 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  acc_conn_test ( ms, false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 50, 10,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 0, 20 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  acc_conn_test ( ms, false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 0, 20,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, two-way, interval 30, 10 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  acc_conn_test ( ms, true, 30, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 30, 10,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
           "[net_ip] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  acc_conn_test ( ms, true, 0, 10, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 0, 10, 
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, one-way, interval 50, 1 connector or pair", 
           "[net_ip] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 50, 1,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, one-way, interval 0, 25 connectors or pairs", 
           "[net_ip] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  acc_conn_test ( ms, false, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 0, 25,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, two-way, interval 20, 15 connectors or pairs", 
           "[net_ip] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  acc_conn_test ( ms, true, 20, 15, 
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 20, 15,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, two-way, interval 0, 15 connectors or pairs, many msgs", 
           "[net_ip] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  acc_conn_test ( ms, true, 0, 15, 
             std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 0, 15, 
             make_empty_lf_text_msg() );

}
