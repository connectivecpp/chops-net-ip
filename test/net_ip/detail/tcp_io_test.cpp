/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include "asio/ip/tcp.hpp"
#include "asio/connect.hpp"
#include "asio/io_context.hpp"

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move, std::pair
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>

#include <cassert>

#include "net_ip/detail/tcp_io.hpp"

#include "net_ip_component/worker.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/msg_handling_start_funcs.hpp"

#include "net_ip/endpoints_resolver.hpp"

#include "marshall/shared_buffer.hpp"

#include <iostream>

using namespace chops::test;

const char*   test_port = "30434";
const char*   test_addr = "";
constexpr int num_msgs = 50;

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

using notify_prom_type = std::promise<std::error_code>;

struct notify_me {
  std::shared_ptr<notify_prom_type>  m_prom;

  notify_me(notify_prom_type prom) : m_prom(std::make_shared<notify_prom_type>(std::move(prom))) { }

  void operator()(std::error_code e, chops::net::detail::tcp_io_shared_ptr p) {
    m_prom->set_value(e);
  }
};

using conn_info = std::pair<std::shared_ptr<chops::net::detail::tcp_io>, std::future<std::error_code>>;

conn_info perform_connect (asio::io_context& ioc) {

  auto res = 
      chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
  asio::ip::tcp::socket sock(ioc);
  asio::connect(sock, *res);

  notify_prom_type notify_prom;
  auto notify_fut = notify_prom.get_future();


  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), 
                                                           notify_me(std::move(notify_prom)));
  return conn_info(iohp, std::move(notify_fut));

}

std::size_t var_conn_func (const vec_buf& var_msg_vec, asio::io_context& ioc, 
                           int interval, std::string_view delim, 
                           const chops::const_shared_buffer& empty_msg) {

  auto info = perform_connect(ioc);
  const auto& iohp = info.first;
  auto& fut = info.second;

  test_counter cnt = 0;
  auto r = tcp_start_io(chops::net::tcp_io_interface(iohp), false, delim, cnt);
  assert (r);

  for (const auto& buf : var_msg_vec) {
    iohp->send(buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  auto err = fut.get();
  std::cerr << "TCP IO handler, variable msg conn, err: " << err << ", " << err.message() << std::endl;

  return cnt.load();
}

std::size_t fixed_conn_func (const vec_buf& fixed_msg_vec, asio::io_context& ioc, 
                             int interval) {

  auto info = perform_connect(ioc);
  const auto& iohp = info.first;
  auto& fut = info.second;

  // do a send-only start_io for the connector side
  auto e = iohp->start_io();
  assert (e);

  for (const auto& buf : fixed_msg_vec) {
    iohp->send(buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }

  // wait for acceptor side to kill connection
  auto err = fut.get();
  std::cerr << "TCP IO handler, fixed size conn, err: " << err << ", " << err.message() << std::endl;

  return fixed_msg_vec.size();
}

conn_info perform_accept (asio::ip::tcp::acceptor& acc) {

  notify_prom_type notify_prom;
  auto notify_fut = notify_prom.get_future();

  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(acc.accept()), 
                                                           notify_me(std::move(notify_prom)));
  return conn_info(iohp, std::move(notify_fut));

}

void perform_test (const vec_buf& var_msg_vec, const vec_buf& fixed_msg_vec,
                   bool reply, int interval, std::string_view delim,
                   const chops::const_shared_buffer& empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  auto res = 
      chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
  REQUIRE(res);
  asio::ip::tcp::acceptor acc(ioc, *(res->cbegin()));

  {
    INFO ("Creating var connector asynchronously, msg interval: " << interval);

    auto conn_fut = std::async(std::launch::async, var_conn_func, std::cref(var_msg_vec), 
                                     std::ref(ioc), interval, delim, empty_msg);

    auto info = perform_accept(acc);
    const auto& iohp = info.first;
    auto& fut = info.second;

    test_counter cnt = 0;
    auto r = tcp_start_io(chops::net::tcp_io_interface(iohp), reply, delim, cnt);
    assert (r);

    auto acc_err = fut.get();

    std::cerr << "TCP IO handler, variable msg acc, err: " << acc_err << ", " << 
                 acc_err.message() << std::endl;

    auto conn_cnt = conn_fut.get();

    REQUIRE (var_msg_vec.size() == cnt);
    if (reply) {
      REQUIRE (var_msg_vec.size() == conn_cnt);
    }
  }
  {
    INFO ("Creating fixed size connector asynchronously, msg interval: " << interval);

    auto conn_fut = std::async(std::launch::async, fixed_conn_func, std::cref(fixed_msg_vec), 
                                     std::ref(ioc), interval);

    auto info = perform_accept(acc);
    const auto& iohp = info.first;
    auto& fut = info.second;

    test_counter cnt = 0;
    test_prom prom;
    auto mh_fut = prom.get_future();
    REQUIRE (iohp->start_io(fixed_size_buf_size, 
                       tcp_fixed_size_msg_hdlr(std::move(prom), fixed_msg_vec.size(), cnt)));

    auto mh_cnt = mh_fut.get(); // pops when cnt max reached
    iohp->stop_io(); // should cause notify future to pop
    auto acc_err = fut.get();

    std::cerr << "TCP IO handler, fixed size acc, err: " << acc_err << ", " << 
                 acc_err.message() << std::endl;

    auto conn_cnt = conn_fut.get();

    REQUIRE (fixed_msg_vec.size() == cnt);

  }

  wk.reset();

}


TEST_CASE ( "Tcp IO handler test, variable len header msgs, one-way, interval 50",
            "[tcp_io] [var_len_msg] [one-way] [interval_50]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', num_msgs),
                 make_fixed_size_msg_vec(num_msgs),
                 false, 50,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp IO handler test, variable len header msgs, one-way, interval 0",
            "[tcp_io] [var_len_msg] [one-way] [interval_0]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*num_msgs),
                 make_fixed_size_msg_vec(2*num_msgs),
                 false, 0,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp IO handler test, variable len header msgs, two-way, interval 50",
            "[tcp_io] [var_len_msg] [two_way] [interval_50]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', num_msgs),
                 make_fixed_size_msg_vec(num_msgs),
                 true, 50,
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp IO handler test, variable len header msgs, two-way, interval 0, many msgs",
            "[tcp_io] [var_len_msg] [two_way] [interval_0] [many]" ) {

  perform_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 50*num_msgs),
                 make_fixed_size_msg_vec(50*num_msgs),
                 true, 0, 
                 std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Tcp IO handler test, CR / LF msgs, one-way, interval 50",
            "[tcp_io] [cr_lf_msg] [one-way] [interval_50]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', num_msgs),
                 make_fixed_size_msg_vec(num_msgs),
                 false, 50,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, CR / LF msgs, one-way, interval 0",
            "[tcp_io] [cr_lf_msg] [one-way] [interval_0]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*num_msgs),
                 make_fixed_size_msg_vec(4*num_msgs),
                 false, 0,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, CR / LF msgs, two-way, interval 30",
            "[tcp_io] [cr_lf_msg] [two-way] [interval_30]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*num_msgs),
                 make_fixed_size_msg_vec(5*num_msgs),
                 true, 30,
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, CR / LF msgs, two-way, interval 0, many msgs",
            "[tcp_io] [cr_lf_msg] [two-way] [interval_0] [many]" ) {

  perform_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 100*num_msgs),
                 make_fixed_size_msg_vec(100*num_msgs),
                 true, 0, 
                 std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, LF msgs, one-way, interval 50",
            "[tcp_io] [lf_msg] [one_way] [interval_50]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', num_msgs),
                 make_fixed_size_msg_vec(num_msgs),
                 false, 50,
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, LF msgs, one-way, interval 0",
            "[tcp_io] [lf_msg] [one_way] [interval_0]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*num_msgs),
                 make_fixed_size_msg_vec(6*num_msgs),
                 false, 0,
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, LF msgs, two-way, interval 20",
            "[tcp_io] [lf_msg] [two_way] [interval_20]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*num_msgs),
                 make_fixed_size_msg_vec(2*num_msgs),
                 true, 20,
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Tcp IO handler test, LF msgs, two-way, interval 0, many msgs",
            "[tcp_io] [lf_msg] [two_way] [interval_0] [many]" ) {

  perform_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 200*num_msgs),
                 make_fixed_size_msg_vec(200*num_msgs),
                 true, 0, 
                 std::string_view("\n"), make_empty_lf_text_msg() );

}

