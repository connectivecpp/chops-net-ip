/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_acceptor detail class.
 *
 *  This test is similar to the tcp_io_test code, with a little bit less
 *  internal plumbing, and allowing multiple connector threads to be started. 
 *  The TCP acceptor is the Chops Net IP class, but the connector threads are 
 *  using blocking Asio connects and io.
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

#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "asio/read.hpp"
#include "asio/buffer.hpp"
#include "asio/io_context.hpp"
#include "asio/connect.hpp"

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>

#include <cassert>

#include "net_ip/detail/tcp_acceptor.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "net_ip/io_type_decls.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/start_funcs.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/repeat.hpp"


// #include <iostream>

using namespace chops::test;

const char* test_port = "30434";
const char* test_host = "";
constexpr int NumMsgs = 50;


// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

std::error_code read_until_err(asio::ip::tcp::socket& sock) {
  char c;
  std::error_code ec;
  auto sz = asio::read(sock, asio::mutable_buffer(&c, 1), ec); // block on read until connection closed
  return ec;
}

std::error_code conn_read_only_func(asio::io_context& ioc) {
  asio::ip::tcp::socket sock(ioc);
  auto endp_seq = *(chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_host, test_port));
  asio::ip::tcp::endpoint endp = asio::connect(sock, endp_seq);
  return read_until_err(sock);
}

void start_conn_read_only_funcs (asio::io_context& ioc, int num_conns) {

  std::vector<std::future<std::error_code>> conn_futs;
  chops::repeat(num_conns, [&ioc, &conn_futs] () {
      conn_futs.emplace_back(std::async(std::launch::async, conn_read_only_func, std::ref(ioc)));
    }
  );
  for (auto& fut : conn_futs) {
    auto e = fut.get(); // wait for connectors to finish
    std::cerr << "Read only future popped, err code: " << e.message() << std::endl;
  }
}

std::size_t conn_data_func (const vec_buf& in_msg_vec, asio::io_context& ioc, 
                            bool read_reply, int interval, chops::const_shared_buffer empty_msg) {

  asio::ip::tcp::socket sock(ioc);
  auto endp_seq = *(chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_host, test_port));
  asio::ip::tcp::endpoint endp = asio::connect(sock, endp_seq);

  std::size_t cnt = 0;
  chops::mutable_shared_buffer return_msg { };
  for (auto buf : in_msg_vec) {
    asio::write(sock, asio::const_buffer(buf.data(), buf.size()));
    if (read_reply) {
      return_msg.resize(buf.size());
      asio::read(sock, asio::mutable_buffer(return_msg.data(), return_msg.size()));
      ++cnt;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  asio::write(sock, asio::const_buffer(empty_msg.data(), empty_msg.size()));
  read_until_err(sock);
  return cnt;
}

std::size_t start_conn_data_funcs (const vec_buf& in_msg_vec, asio::io_context& ioc,
                                   bool reply, int interval, int num_conns,
                                   std::string_view delim, chops::const_shared_buffer empty_msg) {

  std::size_t conn_cnt = 0;
  std::vector<std::future<std::size_t>> conn_futs;

  chops::repeat(num_conns, [&] () {
      conn_futs.emplace_back(std::async(std::launch::async, conn_data_func, std::cref(in_msg_vec), 
                             std::ref(ioc), reply, interval, empty_msg));

    }
  );
  for (auto& fut : conn_futs) {
    conn_cnt += fut.get(); // wait for connectors to finish
  }
  return conn_cnt;
}


void acceptor_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
                         std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                  std::string_view(test_port), std::string_view(), true);

        REQUIRE_FALSE(acc_ptr->is_started());

        chops::net::err_wait_q err_wq;
        auto err_fut = std::async(std::launch::async,
          chops::net::ostream_error_sink_with_wait_queue,
          std::ref(err_wq), std::ref(std::cerr));

        test_counter recv_cnt = 0;
        acc_ptr->start(
          [reply, delim, &recv_cnt] (chops::net::tcp_io_interface io, std::size_t num, bool starting )->bool {
            if (starting) {
              auto r = tcp_start_io(io, reply, delim, recv_cnt);
              assert (r);
            }
            return true;
          },
          chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
        );
        REQUIRE(acc_ptr->is_started());

        auto conn_cnt = start_conn_data_funcs(in_msg_vec, ioc, reply, interval, num_conns,
                                                     delim, empty_msg);
        INFO ("First iteration of connector futures popped, starting second iteration");

        conn_cnt += start_conn_data_funcs(in_msg_vec, ioc, reply, interval, num_conns,
                                          delim, empty_msg);
        INFO ("Second iteration of connector futures popped");

        acc_ptr->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        INFO ("Acceptor stopped");
        REQUIRE_FALSE(acc_ptr->is_started());

        std::size_t total_msgs = 2 * num_conns * in_msg_vec.size();
        REQUIRE (total_msgs == recv_cnt);
        if (reply) {
          REQUIRE (total_msgs == conn_cnt);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        INFO ("Start and stop tests starting");

        acc_ptr->start(
          [&err_wq] (chops::net::tcp_io_interface io, std::size_t num, bool starting )->bool {
            return num < 4u; // when fourth connect happens, return false
          },
          chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
        );
        REQUIRE(acc_ptr->is_started());
        start_conn_read_only_funcs(ioc, 4);
        // connections have been made and disconnects happened from acceptor
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        acc_ptr->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        INFO ("Acceptor stopped");
        REQUIRE_FALSE(acc_ptr->is_started());

        INFO ("Waiting on error wait queue");
        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto cnt = err_fut.get();
        INFO ("Number of messages passed thru error queue: " << cnt);
      }
    }
  } // end given
  wk.reset();

}

SCENARIO ( "Tcp acceptor test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                  false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                  false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                  true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                  true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 0, 60 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_60] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 50*NumMsgs),
                  true, 0, 60, 
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Whaaaat", 'T', NumMsgs),
                  false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                  false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                  false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, two-way, interval 30, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                  true, 30, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, two-way, interval 0, 20 connectors, many msgs", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_0] [connectors_20] [many]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 50*NumMsgs),
                  true, 0, 20, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                  false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, one-way, interval 0, 25 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                  false, 0, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, two-way, interval 20, 25 connectors", 
           "[tcp_acc] [lf_msg] [two_way] [interval_20] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs),
                  true, 20, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, two-way, interval 0, 25 connectors, many msgs", 
           "[tcp_acc] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 80*NumMsgs),
                  true, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}
