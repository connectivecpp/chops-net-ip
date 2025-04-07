/** @file
 *
 * @brief Test scenarios for @c net_entity class.
 *
 * @author Cliff Green
 *
 * @copyright (c) 2018-2025 by Cliff Green
 *
 * Distributed under the Boost Software License, Version 1.0. 
 * (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <memory> // std::shared_ptr
#include <functional> // std::ref
#include <system_error> // std::error_code
#include <set>
#include <chrono>
#include <thread>
#include <future>
#include <cstddef> // std::size_t
#include <iostream> // std::cerr

#include <cassert>

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/io_type_decls.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/msg_handling_start_funcs.hpp"

using namespace chops::test;
using tcp_out_wq = chops::wait_queue<chops::net::tcp_io_output>;

const char* test_port_udp = "30555";
const char* test_host_udp = "127.0.0.1";
const char* test_port_tcp1 = "30556";
const char* test_port_tcp2 = "30557";
const char* test_host_tcp = "";
constexpr int num_msgs = 2000;
constexpr std::chrono::milliseconds tout { 400 };

template <typename IOT>
struct no_start_io_state_chg {
  bool called = false;
  void operator() (chops::net::basic_io_interface<IOT>, std::size_t, bool) {
    called = true;
  }
};

template <typename S>
struct socket_visitor {
  // S is one of asio::ip::udp::socket, asio::ip::tcp::socket, asio::ip::tcp::acceptor&
  bool called = false;
  void operator() (S& sock) { called = true; }
};

template <typename IOT>
struct io_output_visitor {
  bool called = false;
  void operator() (chops::net::basic_io_output<IOT> io) { called = true; }
};

// having problems with GIVEN, WHEN, etc, so just REQUIREs here for now
TEST_CASE ( "Net entity default construction", "[net_entity]" ) {

  chops::net::net_entity net_ent { };

  REQUIRE_FALSE (net_ent.is_valid());
  REQUIRE_FALSE (net_ent.is_started());
  REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::udp::socket>()));
  REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::socket>()));
  REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::acceptor>()));
  REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::tcp_io>()));
  REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::udp_io>()));
  REQUIRE_FALSE (net_ent.start(no_start_io_state_chg<chops::net::udp_io>(), 
                               chops::net::udp_empty_error_func));
  REQUIRE_FALSE (net_ent.start(no_start_io_state_chg<chops::net::tcp_io>(), 
                               chops::net::tcp_empty_error_func));
  REQUIRE_FALSE (net_ent.stop());

}

template <typename IOT, typename S>
void test_methods (chops::net::net_entity net_ent, chops::net::err_wait_q& err_wq) {

  REQUIRE (net_ent.is_valid());
  auto r1 = net_ent.is_started();
  REQUIRE (r1);
  REQUIRE_FALSE (*r1);
  auto r2 = net_ent.stop();
  REQUIRE_FALSE (r2);
  INFO ("Expected err, should be net entity not started: " << r2.error().message());

  REQUIRE (net_ent.start(no_start_io_state_chg<IOT>(), 
           chops::net::make_error_func_with_wait_queue<IOT>(err_wq)));
  r1 = net_ent.is_started();
  REQUIRE (*r1);
  socket_visitor<S> sv;
  REQUIRE_FALSE (sv.called);
  REQUIRE (net_ent.visit_socket(sv));
  REQUIRE (sv.called);

  io_output_visitor<IOT> iov;
  REQUIRE_FALSE (iov.called);
  auto r3 = net_ent.visit_io_output(iov);
  REQUIRE (r3);
  REQUIRE (*r3 == 0u);

  REQUIRE (net_ent.stop());
}

void test_tcp_msg_send (const vec_buf& in_msg_vec,
                        chops::net::net_entity net_acc, chops::net::net_entity net_conn,
                        chops::net::err_wait_q& err_wq) {
  REQUIRE (net_acc.is_valid());
  REQUIRE (net_conn.is_valid());
  REQUIRE_FALSE (*(net_acc.is_started()));
  REQUIRE_FALSE (*(net_conn.is_started()));

  tcp_out_wq out_wq;

  test_counter acc_cnt = 0;
  test_counter conn_cnt = 0;

  REQUIRE ( net_conn.start ( [&out_wq, &conn_cnt, &err_wq]
                     (chops::net::tcp_io_interface io, std::size_t num, bool starting ) {
        if (starting) {
          auto r = tcp_start_io(io, false, "", conn_cnt);
          assert (r);
        }
        out_wq.push(*(io.make_io_output()));
      },
    chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
  ) );

  REQUIRE ( net_acc.start ( [&acc_cnt, &err_wq] 
                    (chops::net::tcp_io_interface io, std::size_t num, bool starting) {
        if (starting) {
          auto r = tcp_start_io(io, true, "", acc_cnt);
          assert (r);
        }
      },
    chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
  ) );

  REQUIRE (*(net_acc.is_started()));
  REQUIRE (*(net_conn.is_started()));

  auto io_out = *(out_wq.wait_and_pop());

  for (const auto& buf : in_msg_vec) {
    io_out.send(buf);
  }

  io_out.send(make_empty_variable_len_msg());

  // wait for another io_output to signal end of processing
  io_out = *(out_wq.wait_and_pop());

  net_conn.stop();
  net_acc.stop();

  REQUIRE (in_msg_vec.size() == acc_cnt);
  REQUIRE (in_msg_vec.size() == conn_cnt);

}

void test_udp_msg_send (const vec_buf& in_msg_vec,
                        chops::net::net_entity net_udp_recv, chops::net::net_entity net_udp_send,
                        chops::net::err_wait_q& err_wq,
                        const asio::ip::udp::endpoint& dest_endp) {

  REQUIRE (net_udp_recv.is_valid());
  REQUIRE (net_udp_send.is_valid());
  REQUIRE_FALSE (*(net_udp_recv.is_started()));
  REQUIRE_FALSE (*(net_udp_send.is_started()));

  test_counter recv_cnt = 0;
  test_counter send_cnt = 0;

  REQUIRE ( net_udp_recv.start ( [&recv_cnt, &err_wq]
                     (chops::net::udp_io_interface io, std::size_t num, bool starting ) {
        if (starting) {
          auto r = udp_start_io(io, false, recv_cnt);
          assert (r);
        }
      },
    chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq)
  ) );

  REQUIRE ( net_udp_send.start ( [&send_cnt, &err_wq, &dest_endp] 
                    (chops::net::udp_io_interface io, std::size_t num, bool starting) {
        if (starting) {
          auto r = udp_start_io(io, false, send_cnt, dest_endp);
          assert (r);
        }
      },
    chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq)
  ) );

  for (const auto& buf : in_msg_vec) {
    auto r = net_udp_send.visit_io_output([buf] (chops::net::udp_io_output io) { io.send(buf); } );
    assert (r);
  }
  auto res = net_udp_send.visit_io_output([] (chops::net::udp_io_output io) {
      io.send(make_empty_variable_len_msg());
    }
  );
  assert (res);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  net_udp_send.stop();
  net_udp_recv.stop();


  REQUIRE_FALSE (*(net_udp_send.is_started()));
  REQUIRE_FALSE (*(net_udp_recv.is_started()));

  REQUIRE (in_msg_vec.size() == recv_cnt);

}

void check_set (const std::set<chops::net::net_entity>& ent_set,
                const chops::net::net_entity& ne_def,
                const chops::net::net_entity& ne_udp,
                const chops::net::net_entity& ne_acc,
                const chops::net::net_entity& ne_conn) {
  REQUIRE (ent_set.size() == 4u);
  auto i = ent_set.cbegin();
  REQUIRE_FALSE (i->is_valid());
  REQUIRE (*i == ne_def); ++i;
  REQUIRE (i->is_valid());
  REQUIRE (*i == ne_udp); ++i;
  REQUIRE (i->is_valid());
  REQUIRE (*i == ne_acc); ++i;
  REQUIRE (i->is_valid());
  REQUIRE (*i == ne_conn); ++i;
}

void comparison_test(const chops::net::net_entity& ne_def,
                     const chops::net::net_entity& ne_udp,
                     const chops::net::net_entity& ne_acc,
                     const chops::net::net_entity& ne_conn) {


  REQUIRE_FALSE (ne_def == ne_udp);
  REQUIRE_FALSE (ne_def == ne_acc);
  REQUIRE_FALSE (ne_def == ne_conn);
  REQUIRE_FALSE (ne_udp == ne_def);
  REQUIRE_FALSE (ne_udp == ne_acc);
  REQUIRE_FALSE (ne_udp == ne_conn);
  REQUIRE_FALSE (ne_acc == ne_def);
  REQUIRE_FALSE (ne_acc == ne_udp);
  REQUIRE_FALSE (ne_acc == ne_conn);
  REQUIRE_FALSE (ne_conn == ne_def);
  REQUIRE_FALSE (ne_conn == ne_udp);
  REQUIRE_FALSE (ne_conn == ne_acc);

  chops::net::net_entity def(ne_def);
  REQUIRE (ne_def == def);
  chops::net::net_entity acc(ne_acc);
  REQUIRE (ne_acc == acc);
  chops::net::net_entity conn(ne_conn);
  REQUIRE (ne_conn == conn);

  REQUIRE (ne_def < ne_udp);
  REQUIRE (ne_def < ne_acc);
  REQUIRE (ne_def < ne_conn);
  REQUIRE (ne_udp < ne_acc);
  REQUIRE (ne_udp < ne_conn);
  REQUIRE (ne_acc < ne_conn);
  REQUIRE_FALSE (ne_conn < ne_acc);
  REQUIRE_FALSE (ne_conn < ne_udp);
  REQUIRE_FALSE (ne_conn < ne_def);
  REQUIRE_FALSE (ne_acc < ne_udp);
  REQUIRE_FALSE (ne_acc < ne_def);
  REQUIRE_FALSE (ne_udp < ne_def);
  std::set<chops::net::net_entity> a_set1 { ne_conn, ne_acc, ne_def, ne_udp };
  check_set(a_set1, ne_def, ne_udp, ne_acc, ne_conn);
  std::set<chops::net::net_entity> a_set2
            { ne_conn, ne_conn, ne_acc, ne_def, ne_udp, ne_acc, ne_acc, ne_def, ne_def };
        check_set(a_set2, ne_def, ne_udp, ne_acc, ne_conn);
}

TEST_CASE ( "Net entity method and comparison testing, UDP entity, TCP acceptor, TCP connector", 
            "[net_entity] [udp_entity] [tcp_acceptor] [tcp_connector]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, 
  chops::net::ostream_error_sink_with_wait_queue, std::ref(err_wq), std::ref(std::cerr));


  {
    auto sp = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                                                     std::string_view(test_port_tcp1),
                                                     std::string_view(test_host_tcp),
                                                     chops::net::simple_timeout(tout), false);
    chops::net::net_entity ne_conn(sp);
    test_methods<chops::net::tcp_io, asio::ip::tcp::socket>(ne_conn, err_wq);
  }

  {
    auto sp = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                                     test_port_tcp1, test_host_tcp, true );
    chops::net::net_entity ne_acc(sp);
    test_methods<chops::net::tcp_io, asio::ip::tcp::acceptor>(ne_acc, err_wq);
  }

  {
    auto sp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, test_port_udp, test_host_udp );
    chops::net::net_entity ne_udp_recv(sp);
    test_methods<chops::net::udp_io, asio::ip::udp::socket>(ne_udp_recv, err_wq);
  }

  {
    auto msg_vec = make_msg_vec (make_variable_len_msg, "Having fun?", 'F', num_msgs);
    auto sp_conn = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                                                        std::string_view(test_port_tcp2),
                                                        std::string_view(test_host_tcp),
                                                        chops::net::simple_timeout(tout), false);
    chops::net::net_entity ne_conn(sp_conn);
    auto sp_acc = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, 
                                               test_port_tcp2, test_host_tcp, true );
    chops::net::net_entity ne_acc(sp_acc);
    test_tcp_msg_send(msg_vec, ne_acc, ne_conn, err_wq);

    auto sp_udp_recv = std::make_shared<chops::net::detail::udp_entity_io>(ioc, 
                                               test_port_udp, test_host_udp );
    chops::net::net_entity ne_udp_recv(sp_udp_recv);
    auto sp_udp_send = std::make_shared<chops::net::detail::udp_entity_io>(ioc, asio::ip::udp::endpoint());
    chops::net::net_entity ne_udp_send(sp_udp_send);

    test_udp_msg_send(msg_vec, ne_udp_recv, ne_udp_send, err_wq, 
                      make_udp_endpoint(test_host_udp, std::stoi(std::string(test_port_udp))));

    comparison_test(chops::net::net_entity(), ne_udp_recv, ne_acc, ne_conn);
  }
  
  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages in sink: " << err_cnt);

  wk.reset();

}

