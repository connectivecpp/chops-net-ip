/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_connector detail class.
 *
 *  This test is similar to the tcp_acceptor_test code, except the Chops Net IP 
 *  tcp_connector class instead of Networking TS blocking io calls.
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

#include "asio/ip/tcp.hpp"
#include "asio/buffer.hpp"
#include "asio/io_context.hpp"

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

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"

#include "net_ip/component/worker.hpp"
#include "net_ip/component/send_to_all.hpp"

#include "net_ip/shared_utility_test.hpp"
#include "net_ip/shared_utility_func_test.hpp"

#include "net_ip/endpoints_resolver.hpp"

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"

#include <iostream> // std::cerr for error sink

using namespace chops::test;

const char* test_port = "30777";
const char* test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 100;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

void start_connectors(const vec_buf& in_msg_vec, asio::io_context& ioc, 
                      int interval, int num_conns,
                      std::string_view delim, chops::const_shared_buffer empty_msg,
                      test_counter& conn_cnt, chops::net::err_wait_q& err_wq) {

  chops::net::send_to_all<chops::net::tcp_io> sta { };

  std::vector<chops::net::detail::tcp_connector_ptr> connectors;
  std::vector<chops::net::tcp_io_interface_future> conn_fut_vec;

  chops::repeat(num_conns, [&] () {
      auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                         std::string_view(test_port), std::string_view(test_host),
                         std::chrono::milliseconds(ReconnTime));

      connectors.push_back(conn_ptr);

      auto conn_futs = get_tcp_io_futures(chops::net::tcp_connector_net_entity(conn_ptr), err_wq,
                                          false, delim, conn_cnt);

      auto conn_start_io = conn_futs.start_fut.get();
      sta.add_io_interface(conn_start_io);
      conn_fut_vec.emplace_back(std::move(conn_futs.stop_fut));
    }
  );
  // send messages through all of the connectors
  for (const auto& buf : in_msg_vec) {
    sta.send(buf);
  }

  auto qs = sta.get_total_output_queue_stats();
std::cerr << "****** Connectors total output queue size: " << qs.output_queue_size << std::endl;
  while (qs.output_queue_size > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    qs = sta.get_total_output_queue_stats();
std::cerr << "****** Connectors total output queue size: " << qs.output_queue_size << std::endl;
  }

  sta.send(empty_msg);

  // wait for stop state change
  for (auto& fut : conn_fut_vec) {
    auto io = fut.get();
  }
}

void acc_conn_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endp_seq = 
            chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_host, test_port);

        auto acc_ptr = 
            std::make_shared<chops::net::detail::tcp_acceptor>(ioc, *(endp_seq.cbegin()), true);
        chops::net::tcp_acceptor_net_entity acc_ent(acc_ptr);

        INFO ("Acceptor created");

        chops::net::err_wait_q err_wq;

        auto err_fut = std::async(std::launch::async, 
          chops::net::ostream_error_sink_with_wait_queue, 
          std::ref(err_wq), std::ref(std::cerr));

        test_counter acc_cnt = 0;
        start_tcp_acceptor(acc_ent, err_wq, reply, delim, acc_cnt);
        REQUIRE(acc_ent.is_started());

        test_counter conn_cnt = 0;

        INFO ("Creating first iteration of connectors and futures, num: " << num_conns);
        start_connectors(in_msg_vec, ioc, interval, num_conns,
                      delim, empty_msg, conn_cnt, err_wq);
        INFO ("Creating second iteration of connectors and futures");
        start_connectors(in_msg_vec, ioc, interval, num_conns,
                      delim, empty_msg, conn_cnt, err_wq);

        acc_ent.stop();
        INFO ("Acceptor stopped");

        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto err_cnt = err_fut.get();
        INFO ("Num err messages in sink: " << err_cnt);

        std::size_t total_msgs = 2 * num_conns * in_msg_vec.size();
        REQUIRE (total_msgs == acc_cnt);
        if (reply) {
          REQUIRE (total_msgs == conn_cnt);
        }
      }
    }
  } // end given
  wk.reset();

}

SCENARIO ( "Tcp connector test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                  false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                  false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );
}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 50, 1 connector", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                  true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 1 connector, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', 100*NumMsgs),
                  true, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, one-way, interval 0, 2 connectors", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_2]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', NumMsgs),
                  false, 0, 2,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 2 connectors", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_2]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', NumMsgs),
                  true, 0, 2,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                  true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 30 connectors, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_30] [many]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs),
                  true, 0, 30,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs),
                  false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                  false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                  false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, two-way, interval 30, 20 connectors", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                  true, 30, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
                  true, 0, 10, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                  false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, one-way, interval 0, 25 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                  false, 0, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, two-way, interval 0, 25 connectors, many msgs", 
           "[tcp_conn] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs),
                  true, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

