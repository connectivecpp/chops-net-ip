/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_connector detail class.
 *
 *  This test is similar to the tcp_acceptor_test code, except the Chops Net IP 
 *  tcp_connector class instead of Asio blocking io calls.
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
#include "asio/buffer.hpp"
#include "asio/io_context.hpp"

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move, std::ref
#include <thread>
#include <future> // std::async
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>

#include <cassert>

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/io_type_decls.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/error_delivery.hpp"
#include "net_ip_component/output_queue_stats.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/start_funcs.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/repeat.hpp"
#include "queue/wait_queue.hpp"

#include <iostream> // std::cerr for error sink

using namespace chops::test;

const char* test_port_var = "30777";
const char* test_port_fixed = "30778";
const char* test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 800;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

using io_wait_q = chops::wait_queue<chops::net::basic_io_output<chops::net::tcp_io> >;

struct no_start_io_state_chg {
  bool operator() (chops::net::tcp_io_interface, std::size_t, bool) {
    return true;
  }
};

std::size_t start_fixed_connectors(int num_conns, std::size_t expected_cnt,
                                   test_counter& conn_cnt, chops::net::err_wait_q& err_wq) {

  // create another executor for a little more concurrency
  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  std::size_t sum = 0u;
  {
    std::vector<chops::net::detail::tcp_connector_shared_ptr> connectors;
    std::vector<std::future<std::size_t>> conn_futs;

    chops::repeat(num_conns, [&connectors, &conn_futs, &conn_cnt, &err_wq, &ioc, expected_cnt] () {
        auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                           std::string_view(test_port_fixed), std::string_view(test_host),
                           std::chrono::milliseconds(ReconnTime));

        connectors.push_back(conn_ptr);
        test_prom prom;
        conn_futs.push_back(std::move(prom.get_future()));

        auto r = conn_ptr->start( [&conn_cnt, expected_cnt, &err_wq, &prom]
                         (chops::net::tcp_io_interface io, std::size_t num, bool starting )->bool {
              if (starting) {
                auto r = io.start_io(fixed_size_buf_size,
                                     tcp_fixed_size_msg_hdlr(std::move(prom), expected_cnt, conn_cnt));
                assert(r);
              }
              return true;
            },
          chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
        );
        assert (!r);
std::cerr << "After fixed conn start, error, message: " << r << ", " << r.message() << std::endl;
      }
    );

    // wait for futures to pop
    for (auto& fut : conn_futs) {
      sum += fut.get();
    }
    for (auto& p : connectors) {
      p->stop();
    }
  }

  wk.reset();
  return sum;
}


void start_stop_connector(asio::io_context& ioc, int interval, chops::net::err_wait_q& err_wq) {

  auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                     std::string_view(test_port_var), std::string_view(test_host),
                     std::chrono::milliseconds(0)); // no reconnect logic

  auto r1 = conn_ptr->start( no_start_io_state_chg(), 
                             chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
  REQUIRE_FALSE(r1);
  std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  auto r2 = conn_ptr->stop();
  REQUIRE_FALSE(r2);
  auto r3 = conn_ptr->start( no_start_io_state_chg(), 
                             chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
  REQUIRE (r3);
  INFO ("Start after stop error code: " << r3.message());
}
                                  
std::size_t start_var_connectors(const vec_buf& in_msg_vec,
                                 int interval, int num_conns,
                                 std::string_view delim, chops::const_shared_buffer empty_msg,
                                 test_counter& conn_cnt, chops::net::err_wait_q& err_wq) {

  // create another executor for a little more concurrency
  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  io_wait_q io_wq;

  {
    std::vector<chops::net::detail::tcp_connector_shared_ptr> connectors;

    chops::repeat(num_conns, [&connectors, &io_wq, delim, &conn_cnt, &err_wq, &ioc] () {
        auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                           std::string_view(test_port_var), std::string_view(test_host),
                           std::chrono::milliseconds(ReconnTime));

        connectors.push_back(conn_ptr);

        auto r = conn_ptr->start( [&io_wq, delim, &conn_cnt, &err_wq]
                         (chops::net::tcp_io_interface io, std::size_t num, bool starting )->bool {
              if (starting) {
                auto r = tcp_start_io(io, false, delim, conn_cnt);
                assert(r);
                io_wq.push(*(io.make_io_output()));
                return true;
              }
              io_wq.push(*(io.make_io_output()));
              return false; // stop connector upon TCP connection going down
            },
          chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
        );
        assert (!r);
      }
    );
    // get all of the io_output objects
    std::vector<chops::net::tcp_io_output> io_outs;
    chops::repeat(num_conns, [&io_wq, &io_outs, &in_msg_vec, interval] () {
        auto io = *(io_wq.wait_and_pop()); // will hang if num io_outputs popped doesn't match num pushed
        io_outs.push_back(io);
        // send messages through this connector
        for (const auto& buf : in_msg_vec) {
          io.send(buf);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
      }
    );

    // poll output queue size of all handlers until 0
    chops::net::accumulate_output_queue_stats_until(io_outs.cbegin(), io_outs.cend(),
          poll_output_queue_cond(200, std::cerr));

    for (auto& io : io_outs) {
      io.send(empty_msg);
    }

    // wait for stop indication
    chops::repeat(num_conns, [&io_wq] () {
        auto a = io_wq.wait_and_pop();
        assert (a);
      }
    );
  }

  wk.reset();

  return io_wq.size();  // should be 0 after stopping
}

void perform_test (const vec_buf& in_msg_vec, const vec_buf& fixed_msg_vec,
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
    test_counter conn_cnt = 0;
    INFO ("First iteration of var connectors, separate thread, before acceptor, num: " << num_conns);

    auto conn_fut = std::async(std::launch::async, start_var_connectors,
            std::cref(in_msg_vec), interval, num_conns,
            delim, empty_msg, std::ref(conn_cnt), std::ref(err_wq));

    INFO ("Pausing 2 seconds to test connector re-connect timeout");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                                               test_port_var, "", true);

    test_counter acc_cnt = 0;
    auto r = acc_ptr->start( [delim, reply, &acc_cnt, &err_wq]
                    (chops::net::tcp_io_interface io, std::size_t num, bool starting )->bool {
          if (starting) {
            auto r = tcp_start_io(io, reply, delim, acc_cnt);
            assert(r);
          }
          return true;
        },
      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
    );
    REQUIRE_FALSE (r);
    REQUIRE(acc_ptr->is_started());
    INFO ("Acceptor created, connectors should now connect");

    REQUIRE(conn_fut.get() == 0u);

    INFO ("Second iteration of var connectors, after acceptor start, not in separate thread");
    auto sz = start_var_connectors(in_msg_vec, interval, num_conns,
                               delim, empty_msg, conn_cnt, err_wq);
    REQUIRE(sz == 0u);

    INFO ("Test simple start and stop of a single connector, not in separate thread");
    start_stop_connector(ioc, interval, err_wq);

    acc_ptr->stop();
    INFO ("Acceptor stopped");

    std::size_t total_msgs = 2 * num_conns * in_msg_vec.size();
    REQUIRE (total_msgs == acc_cnt);
    if (reply) {
      REQUIRE (total_msgs == conn_cnt);
    }
  }

  {
    INFO ("Fixed size msg connectors, starting acceptor, num: " << num_conns);

    std::promise<std::size_t> prom;
    auto start_fut = prom.get_future();
    auto acc_ptr = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                                           test_port_fixed, "", true);
    auto r = acc_ptr->start( [num_conns, &prom]
                    (chops::net::tcp_io_interface io, std::size_t num, bool starting ) mutable {
          if (starting) {
            auto r = io.start_io();
            assert(r);
            if (num == num_conns) {
              prom.set_value(num);
            }
          }
          return true;
        },
      chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
    );
    REQUIRE_FALSE (r);
    REQUIRE (acc_ptr->is_started());

    test_counter conn_cnt = 0;
    auto conn_fut = std::async(std::launch::async, start_fixed_connectors,
                               num_conns, fixed_msg_vec.size(),
                               std::ref(conn_cnt), std::ref(err_wq));
    auto n = start_fut.get();
    REQUIRE (n == num_conns);
    for (const auto& buf : fixed_msg_vec) {
      n = acc_ptr->visit_io_output([&buf] (chops::net::tcp_io_output io) {
          io.send(buf);
        }
      );
std::cerr << "Visit_io_output returned: " << n << std::endl;
      assert (n == num_conns);
    }

    auto t = conn_fut.get();
    REQUIRE (t == 0u);

    acc_ptr->stop();
    INFO ("Acceptor stopped");

    REQUIRE (conn_cnt == (num_conns * fixed_msg_vec.size()));
  }

  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages passed thru error queue: " << err_cnt);

  wk.reset();

}

TEST_CASE ( "Tcp connector test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 false, 50, 1,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                 make_fixed_size_msg_vec(2*NumMsgs),
                 false, 0, 1,
                 std::string_view(), make_empty_variable_len_msg() );
}

TEST_CASE ( "Tcp connector test, var len msgs, two-way, interval 30, 1 connector", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_30] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 true, 30, 1,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, var len msgs, two-way, interval 0, 1 connector, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', 100*NumMsgs),
                 make_fixed_size_msg_vec(100*NumMsgs),
                 true, 0, 1,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, var len msgs, one-way, interval 0, 2 connectors", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_2]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 false, 0, 2,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, var len msgs, two-way, interval 0, 2 connectors", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_2]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 true, 0, 2,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                 make_fixed_size_msg_vec(100*NumMsgs),
                 true, 0, 10,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp connector test, CR / LF msgs, one-way, interval 20, 1 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_20] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 false, 20, 1,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, CR / LF msgs, one-way, interval 30, 10 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_30] [connectors_10]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 false, 30, 10,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                 make_fixed_size_msg_vec(4*NumMsgs),
                 false, 0, 20,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, CR / LF msgs, two-way, interval 10, 5 connectors", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_10] [connectors_5]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                 make_fixed_size_msg_vec(5*NumMsgs),
                 true, 10, 5,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, CR / LF msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
                 make_fixed_size_msg_vec(5*NumMsgs),
                 true, 0, 10, 
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, LF msgs, one-way, interval 40, 1 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_40] [connectors_1]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                 make_fixed_size_msg_vec(NumMsgs),
                 false, 40, 1,
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, LF msgs, one-way, interval 0, 15 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_0] [connectors_15]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                 make_fixed_size_msg_vec(5*NumMsgs),
                 false, 0, 15,
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp connector test, LF msgs, two-way, interval 0, 15 connectors, many msgs", 
           "[tcp_conn] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs),
                 make_fixed_size_msg_vec(5*NumMsgs),
                 true, 0, 15, 
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

