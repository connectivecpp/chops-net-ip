/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity class.
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

#include <memory> // std::shared_ptr
#include <functional> // std::ref
#include <system_error> // std::error_code
#include <set>
#include <chrono>
#include <thread>
#include <future>
#include <cstddef> // std::size_t
#include <iostream> // std::cerr

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
#include "shared_test/start_funcs.hpp"

using namespace chops::test;
using io_wait_q = chops::wait_queue<chops::net::basic_io_output<chops::net::tcp_io> >;

const char* test_port_udp = "30555";
const char* test_port_tcp1 = "30556";
const char* test_port_tcp2 = "30557";
const char* test_host = "";
constexpr int NumMsgs = 30;
constexpr int ReconnTime = 400;

template <typename IOT>
struct io_state_chg {
  bool called = false;
  bool operator() (chops::net::basic_io_interface<IOT>, std::size_t, bool) {
    called = true;
    return true;
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

SCENARIO ( "Net entity default construction", "[net_entity]" ) {

  chops::net::net_entity net_ent { };

  GIVEN ("A default constructed net_entity") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (net_ent.is_valid());
      }
    }
    AND_WHEN ("methods are called on an invalid net_entity") {
      THEN ("an appropriate error code is returned") {
        REQUIRE_FALSE (net_ent.is_started());
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::udp::socket>()));
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::socket>()));
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::acceptor>()));
        REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::tcp_io>()));
        REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::udp_io>()));
        REQUIRE_FALSE (net_ent.start(io_state_chg<chops::net::udp_io>(), 
                                     chops::net::udp_empty_error_func));
        REQUIRE_FALSE (net_ent.start(io_state_chg<chops::net::tcp_io>(), 
                                     chops::net::tcp_empty_error_func));
        REQUIRE_FALSE (net_ent.stop());
      }
    }
  } // end given

}

template <typename IOT, typename S>
void test_methods (chops::net::net_entity net_ent, chops::net::err_wait_q& err_wq) {

// having problems with GIVEN, WHEN, etc, so just REQUIREs here for now
  REQUIRE (net_ent.is_valid());
  auto r1 = net_ent.is_started();
  REQUIRE (r1.has_value());
  REQUIRE_FALSE (*r1);
  auto r2 = net_ent.stop();
  REQUIRE_FALSE (r2.has_value());
  REQUIRE (net_ent.start(io_state_chg<IOT>(), 
           chops::net::make_error_func_with_wait_queue<IOT>(err_wq)));
  auto r3 = net_ent.is_started();
  REQUIRE (*r3);
  socket_visitor<S> sv;
  REQUIRE_FALSE (sv.called);
  REQUIRE (net_ent.visit_socket(sv));
  REQUIRE (sv.called);
  REQUIRE (net_ent.stop());
  io_output_visitor<IOT> iov;
  REQUIRE_FALSE (iov.called);
  auto r4 = net_ent.visit_io_output(iov);
  REQUIRE (r4);
  REQUIRE (*r4 == 0u);
}

void test_tcp_msg_send (const vec_buf& in_msg_vec,
                        chops::net::net_entity net_acc, chops::net::net_entity net_conn,
                        chops::net::err_wait_q& err_wq) {
  REQUIRE (net_acc.is_valid());
  REQUIRE (net_conn.is_valid());
  REQUIRE_FALSE (*(net_acc.is_started()));
  REQUIRE_FALSE (*(net_conn.is_started()));

  io_wait_q io_wq;

  test_counter acc_cnt = 0;
  test_counter conn_cnt = 0;

  net_conn.start ( [&io_wq, &conn_cnt, &err_wq]
                     (chops::net::tcp_io_interface io, std::size_t num, bool starting )-> bool {
        if (starting) {
          auto r = tcp_start_io(io, false, "", conn_cnt);
          io_wq.push(*(io.make_io_output()));
        }
        io_wq.push(*(io.make_io_output()));
        return true;
      },
    chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
  );

  net_acc.start ( [&acc_cnt, &err_wq] 
                    (chops::net::tcp_io_interface io, std::size_t num, bool starting) ->bool {
        if (starting) {
          auto r = tcp_start_io(io, true, "", acc_cnt);
        }
        return true;
      },
    chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq)
  );

  REQUIRE (*(net_acc.is_started()));
  REQUIRE (*(net_conn.is_started()));

  auto io_out = *(io_wq.wait_and_pop());

  for (const auto& buf : in_msg_vec) {
    io_out.send(buf);
  }

  io_out.send(make_empty_variable_len_msg());
  // wait for another io_output to signal end of processing
  io_out = *(io_wq.wait_and_pop());

  net_conn.stop();
  net_acc.stop();

  REQUIRE_FALSE (*(net_acc.is_started()));
  REQUIRE_FALSE (*(net_conn.is_started()));

  REQUIRE (in_msg_vec.size() == acc_cnt);
  REQUIRE (in_msg_vec.size() == conn_cnt);

}

void test_udp_msg_send (const vec_buf& in_msg_vec,
                        chops::net::net_entity net_udp_recv, chops::net::net_entity net_udp_send,
                        chops::net::err_wait_q& err_wq) {
  REQUIRE (net_udp_recv.is_valid());
  REQUIRE (net_udp_send.is_valid());
  REQUIRE_FALSE (*(net_udp_recv.is_started()));
  REQUIRE_FALSE (*(net_udp_send.is_started()));

  test_counter acc_cnt = 0;
  test_counter conn_cnt = 0;





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


  GIVEN ("One default constructed and three different net entities") {
    WHEN ("they are compared for equality") {
      THEN ("none compare equal") {
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
      }
    }
    AND_WHEN ("same net entities or two default net entities are compared for equality") {
      THEN ("they compare equal") {
        chops::net::net_entity def(ne_def);
        REQUIRE (ne_def == def);
        chops::net::net_entity acc(ne_acc);
        REQUIRE (ne_acc == acc);
        chops::net::net_entity conn(ne_conn);
        REQUIRE (ne_conn == conn);
      }
    }
    AND_WHEN ("the net entities are compared less than") {
      THEN ("they compare appropriately") {
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
      }
    }
    AND_WHEN ("a set is created") {
      std::set<chops::net::net_entity> a_set { ne_conn, ne_acc, ne_def, ne_udp };
      THEN ("the net entities are put in the correct order") {
        check_set(a_set, ne_def, ne_udp, ne_acc, ne_conn);
      }
    }
    AND_WHEN ("a set is created with multiple same entities") {
      std::set<chops::net::net_entity> a_set 
            { ne_conn, ne_conn, ne_acc, ne_def, ne_udp, ne_acc, ne_acc, ne_def, ne_def };
      THEN ("same entities are not duplicated") {
        check_set(a_set, ne_def, ne_udp, ne_acc, ne_conn);
      }
    }
  } // end given

}

SCENARIO ( "Net entity method and comparison testing, UDP entity, TCP acceptor, TCP connector", 
           "[net_entity] [udp_entity] [tcp_acceptor] [tcp_connector]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, 
  chops::net::ostream_error_sink_with_wait_queue, std::ref(err_wq), std::ref(std::cerr));


  auto sp_conn1 = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                                                            std::string_view(test_port_tcp1),
                                                            std::string_view(test_host),
                                                            std::chrono::milliseconds(ReconnTime));
  chops::net::net_entity ne_conn1(sp_conn1);
  test_methods<chops::net::tcp_io, asio::ip::tcp::socket>(ne_conn1, err_wq);

  auto sp_acc = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, test_port_tcp2, test_host, true);
  chops::net::net_entity ne_acc(sp_acc);
  test_methods<chops::net::tcp_io, asio::ip::tcp::acceptor>(ne_acc, err_wq);

  auto sp_udp1 = std::make_shared<chops::net::detail::udp_entity_io>(ioc, test_port_udp, test_host);
  chops::net::net_entity ne_udp1(sp_udp1);
  test_methods<chops::net::udp_io, asio::ip::udp::socket>(ne_udp1, err_wq);

  auto msg_vec = make_msg_vec (make_variable_len_msg, "Having fun?", 'F', NumMsgs);

  auto sp_conn2 = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                                                             std::string_view(test_port_tcp2),
                                                             std::string_view(test_host),
                                                             std::chrono::milliseconds(ReconnTime));
  chops::net::net_entity ne_conn2(sp_conn2);

  test_tcp_msg_send(msg_vec, ne_acc, ne_conn2, err_wq);

  auto sp_udp2 = std::make_shared<chops::net::detail::udp_entity_io>(ioc, asio::ip::udp::endpoint());
  chops::net::net_entity ne_udp2(sp_udp2);

  test_udp_msg_send(msg_vec, ne_udp1, ne_udp2, err_wq);

  std::this_thread::sleep_for(std::chrono::seconds(1)); // give network entities time to shut down

  comparison_test(chops::net::net_entity(), ne_udp1, ne_acc, ne_conn1);
  
  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages in sink: " << err_cnt);

  wk.reset();

}


