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
#include "shared_test/msg_handling_start_funcs.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/repeat.hpp"


// #include <iostream>

using namespace chops::test;

const char* test_port = "30434";
const char* test_host = "";
constexpr int num_msgs = 50;


// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

void perform_connect(asio::ip::tcp::socket& sock, asio::io_context& ioc) {
  auto endp_seq = *(chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_host, test_port));
  asio::ip::tcp::endpoint endp = asio::connect(sock, endp_seq);
}

std::error_code read_until_err(asio::ip::tcp::socket& sock) {
  char c;
  std::error_code ec;
  auto sz = asio::read(sock, asio::mutable_buffer(&c, 1), ec); // block on read until connection closed
  return ec;
}

std::error_code read_only_func(asio::io_context& ioc) {
  asio::ip::tcp::socket sock(ioc);
  perform_connect(sock, ioc);
  return read_until_err(sock);
}

void start_read_only_funcs (asio::io_context& ioc, int num_conns) {
  std::vector<std::future<std::error_code>> conn_futs;
  chops::repeat(num_conns, [&ioc, &conn_futs] () {
      conn_futs.emplace_back(std::async(std::launch::async, read_only_func, std::ref(ioc)));
    }
  );
  for (auto& fut : conn_futs) {
    auto e = fut.get(); // wait for connectors to finish
    std::cerr << "Read only future popped, err code: " << e.message() << std::endl;
  }
}

// fixed size data connectors perform receives only, no sends
std::size_t fixed_data_func (asio::io_context& ioc) {

  asio::ip::tcp::socket sock(ioc);
  perform_connect(sock, ioc);

  chops::mutable_shared_buffer incoming_msg { };
  incoming_msg.resize(fixed_size_buf_size);

  std::error_code ec;
  std::size_t cnt = 0;
  while (true) {
    asio::read(sock, asio::mutable_buffer(incoming_msg.data(), fixed_size_buf_size), ec);
    if (ec) {
      break;
    }
    ++cnt;
  }
  return cnt;
}

std::size_t start_fixed_data_funcs (asio::io_context& ioc, int num_conns) {

  std::size_t conn_cnt = 0;
  std::vector<std::future<std::size_t>> conn_futs;

  chops::repeat(num_conns, [&ioc, &conn_futs] () {
      conn_futs.emplace_back(std::async(std::launch::async, fixed_data_func, std::ref(ioc)));

    }
  );
  for (auto& fut : conn_futs) {
    conn_cnt += fut.get(); // wait for connectors to finish
  }
  return conn_cnt;
}

std::size_t var_data_func (const vec_buf& var_msg_vec, asio::io_context& ioc, 
                           bool read_reply, int interval, const chops::const_shared_buffer& empty_msg) {

  asio::ip::tcp::socket sock(ioc);
  perform_connect(sock, ioc);

  std::size_t cnt = 0;
  chops::mutable_shared_buffer return_msg { };
  for (const auto& buf : var_msg_vec) {
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

std::size_t start_var_data_funcs (const vec_buf& var_msg_vec, asio::io_context& ioc,
                                  bool reply, int interval, int num_conns,
                                  std::string_view delim, const chops::const_shared_buffer& empty_msg) {

  std::size_t conn_cnt = 0;
  std::vector<std::future<std::size_t>> conn_futs;

  chops::repeat(num_conns, [&] () {
      conn_futs.emplace_back(std::async(std::launch::async, var_data_func, std::cref(var_msg_vec), 
                             std::ref(ioc), reply, interval, empty_msg));

    }
  );
  for (auto& fut : conn_futs) {
    conn_cnt += fut.get(); // wait for connectors to finish
  }
  return conn_cnt;
}


void acceptor_test (const vec_buf& var_msg_vec, const vec_buf& fixed_msg_vec,
                    bool reply, int interval, int num_conns,
                    std::string_view delim, const chops::const_shared_buffer& empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async,
        chops::net::ostream_error_sink_with_wait_queue,
        std::ref(err_wq), std::ref(std::cerr));

  {
    INFO ("Variable length message tests starting");

    auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                  std::string_view(test_port), std::string_view(), true);
    REQUIRE_FALSE(acc_ptr->is_started());

    test_counter recv_cnt = 0;
    acc_ptr->start( [reply, delim, &recv_cnt] 
                      (chops::net::tcp_io_interface io, std::size_t num, bool starting ) {
        if (starting) {
          auto r = tcp_start_io(io, reply, delim, recv_cnt);
          assert (r);
        }
      },
      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
    );
    REQUIRE(acc_ptr->is_started());

    auto conn_cnt = start_var_data_funcs(var_msg_vec, ioc, reply, interval, num_conns,
                                          delim, empty_msg);
    INFO ("First iteration of connector futures popped, starting second iteration");

    conn_cnt += start_var_data_funcs(var_msg_vec, ioc, reply, interval, num_conns,
                                      delim, empty_msg);
    INFO ("Second iteration of connector futures popped");

    acc_ptr->stop();
    INFO ("Acceptor stopped");
    REQUIRE_FALSE(acc_ptr->is_started());

    std::size_t total_msgs = 2 * num_conns * var_msg_vec.size();
    REQUIRE (total_msgs == recv_cnt);
    if (reply) {
      REQUIRE (total_msgs == conn_cnt);
    }
  }

  {
    INFO ("Fixed size message tests starting, message sending from acceptor to connector");

    auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                  std::string_view(test_port), std::string_view(), true);
    REQUIRE_FALSE(acc_ptr->is_started());

    std::promise<std::size_t> prom;
    auto start_fut = prom.get_future();
    acc_ptr->start( [num_conns, &prom] 
                    (chops::net::tcp_io_interface io, std::size_t num, bool starting ) mutable {
        if (starting) {
          auto r = io.start_io(); // send only
          assert (r);
          if (num == num_conns) {
            prom.set_value(num);
          }
        }
      },
      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
    );
    REQUIRE(acc_ptr->is_started());

    auto conn_fut = std::async(std::launch::async, start_fixed_data_funcs, 
                               std::ref(ioc), num_conns);

    auto n = start_fut.get(); // all connectors have now connected
    REQUIRE (n == num_conns);

    for (const auto& buf : fixed_msg_vec) {
      n = acc_ptr->visit_io_output([&buf] (chops::net::tcp_io_output io) {
          io.send(buf);
        }
      );
      assert (n == num_conns);
    }
    std::size_t sum = 0;
    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      n = acc_ptr->visit_io_output([&sum] (chops::net::tcp_io_output io) {
          auto s = io.get_output_queue_stats();
          assert (s);
          sum += (*s).output_queue_size;
        }
      );
      assert (n == num_conns);
    } while (sum != 0);
    
    acc_ptr->stop();
    INFO ("Acceptor stopped");
    REQUIRE_FALSE(acc_ptr->is_started());

    auto conn_cnt = conn_fut.get();
    REQUIRE (conn_cnt == num_conns * fixed_msg_vec.size());
  }

  {
    INFO ("Start and stop tests starting");

    auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                  std::string_view(test_port), std::string_view(), true);

    std::promise<std::size_t> prom;
    auto start_fut = prom.get_future();
    acc_ptr->start( [&prom] (chops::net::tcp_io_interface io, std::size_t num, bool starting ) {
        if (starting && num == 4u) { // when fourth connect happens, pop the future
          prom.set_value(num);
        }
      },
      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
    );
    REQUIRE(acc_ptr->is_started());
    auto conn_fut = std::async(std::launch::async, start_read_only_funcs, std::ref(ioc), 4);
    auto n = start_fut.get();
    REQUIRE (n == 4u);
    // connections have been made, now force disconnects through stop
    acc_ptr->stop();
    // wait for connectors to be disconnected
    conn_fut.get();
    REQUIRE_FALSE(acc_ptr->is_started());
  }

  INFO ("Waiting on error wait queue");
  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto cnt = err_fut.get();
  INFO ("Number of messages passed thru error queue: " << cnt);

  wk.reset();

}

TEST_CASE ( "Tcp acceptor test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', num_msgs),
                  make_fixed_size_msg_vec(num_msgs),
                  false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp acceptor test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*num_msgs),
                  make_fixed_size_msg_vec(2*num_msgs),
                  false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp acceptor test, var len msgs, two-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', num_msgs),
                  make_fixed_size_msg_vec(num_msgs),
                  true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp acceptor test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 50*num_msgs),
                  make_fixed_size_msg_vec(50*num_msgs),
                  true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp acceptor test, var len msgs, two-way, interval 0, 30 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_30] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 20*num_msgs),
                  make_fixed_size_msg_vec(20*num_msgs),
                  true, 0, 30, 
                  std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Whaaaat", 'T', num_msgs),
                  make_fixed_size_msg_vec(num_msgs),
                  false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', num_msgs),
                  make_fixed_size_msg_vec(num_msgs),
                  false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*num_msgs),
                  make_fixed_size_msg_vec(4*num_msgs),
                  false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test, CR / LF msgs, two-way, interval 30, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 2*num_msgs),
                  make_fixed_size_msg_vec(2*num_msgs),
                  true, 30, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test, CR / LF msgs, two-way, interval 0, 20 connectors, many msgs", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_0] [connectors_20] [many]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 15*num_msgs),
                  make_fixed_size_msg_vec(15*num_msgs),
                  true, 0, 20, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test,  LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', num_msgs),
                  make_fixed_size_msg_vec(num_msgs),
                  false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test,  LF msgs, one-way, interval 0, 25 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*num_msgs),
                  make_fixed_size_msg_vec(6*num_msgs),
                  false, 0, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test,  LF msgs, two-way, interval 20, 25 connectors", 
           "[tcp_acc] [lf_msg] [two_way] [interval_20] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*num_msgs),
                  make_fixed_size_msg_vec(2*num_msgs),
                  true, 20, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp acceptor test,  LF msgs, two-way, interval 0, 25 connectors, many msgs", 
           "[tcp_acc] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 30*num_msgs),
                  make_fixed_size_msg_vec(30*num_msgs),
                  true, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}
