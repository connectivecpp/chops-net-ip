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
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>

#include "net_ip/detail/tcp_io.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/shared_utility_test.hpp"
#include "marshall/shared_buffer.hpp"

// #include <iostream>

using namespace chops::test;

// using notifier_cb = 
//   typename chops::net::detail::io_base<chops::net::detail::tcp_io>::entity_notifier_cb;

const char*   test_port = "30434";
const char*   test_addr = "";
constexpr int NumMsgs = 50;

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

using notify_prom_type = std::promise<std::error_code>;

struct notify_me {
  std::shared_ptr<notify_prom_type>  m_prom;

  notify_me(notify_prom_type prom) : m_prom(std::make_shared<notify_prom_type>(std::move(prom))) { }

  void operator()(std::error_code e, chops::net::detail::tcp_io_ptr p) {
    p->close();
    m_prom->set_value(e);
  }
};

std::size_t connector_func (const vec_buf& in_msg_vec, asio::io_context& ioc, 
                            int interval, std::string_view delim, chops::const_shared_buffer empty_msg) {

  auto endps = 
      chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
  asio::ip::tcp::socket sock(ioc);
  asio::connect(sock, endps);

  notify_prom_type notify_prom;
  auto notify_fut = notify_prom.get_future();


  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), 
                                                           notify_me(std::move(notify_prom)));

  test_counter cnt = 0;
  tcp_start_io(chops::net::tcp_io_interface(iohp), false, delim, cnt);

  for (auto buf : in_msg_vec) {
    iohp->send(buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  auto err = notify_fut.get();

// std::cerr << "Inside connector func, err: " << err << ", " << err.message() << std::endl;

  return cnt.load();
}

void acc_conn_test (const vec_buf& in_msg_vec, bool reply, int interval, std::string_view delim,
                    chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and connector are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endps = 
            chops::net::endpoints_resolver<asio::ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
        asio::ip::tcp::acceptor acc(ioc, *(endps.cbegin()));

        INFO ("Creating connector asynchronously, msg interval: " << interval);

        auto conn_fut = std::async(std::launch::async, connector_func, std::cref(in_msg_vec), 
                                   std::ref(ioc), interval, delim, empty_msg);

        notify_prom_type notify_prom;
        auto notify_fut = notify_prom.get_future();

        auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(acc.accept()), 
                                                                 notify_me(std::move(notify_prom)));
        test_counter cnt = 0;
        tcp_start_io(chops::net::tcp_io_interface(iohp), reply, delim, cnt);

        auto acc_err = notify_fut.get();
// std::cerr << "Inside acc_conn_test, acc_err: " << acc_err << ", " << acc_err.message() << std::endl;

        auto conn_cnt = conn_fut.get();

        REQUIRE (in_msg_vec.size() == cnt);
        if (reply) {
          REQUIRE (in_msg_vec.size() == conn_cnt);
        }
      }
    }
  } // end given

  wk.reset();
//  wk.stop();

}


SCENARIO ( "Tcp IO handler test, variable len msgs, one-way, interval 50",
           "[tcp_io] [var_len_msg] [one-way] [interval_50]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                  false, 50,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, one-way, interval 0",
           "[tcp_io] [var_len_msg] [one-way] [interval_0]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                  false, 0,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, two-way, interval 50",
           "[tcp_io] [var_len_msg] [two_way] [interval_50]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                  true, 50,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, two-way, interval 0, many msgs",
           "[tcp_io] [var_len_msg] [two_way] [interval_0] [many]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                  true, 0, 
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, one-way, interval 50",
           "[tcp_io] [cr_lf_msg] [one-way] [interval_50]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                  false, 50,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, one-way, interval 0",
           "[tcp_io] [cr_lf_msg] [one-way] [interval_0]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                  false, 0,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, two-way, interval 30",
           "[tcp_io] [cr_lf_msg] [two-way] [interval_30]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                  true, 30,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, two-way, interval 0, many msgs",
           "[tcp_io] [cr_lf_msg] [two-way] [interval_0] [many]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
                  true, 0, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, LF msgs, one-way, interval 50",
           "[tcp_io] [lf_msg] [one_way] [interval_50]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                  false, 50,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, LF msgs, one-way, interval 0",
           "[tcp_io] [lf_msg] [one_way] [interval_0]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                  false, 0,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, LF msgs, two-way, interval 20",
           "[tcp_io] [lf_msg] [two_way] [interval_20]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs),
                  true, 20,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp IO handler test, LF msgs, two-way, interval 0, many msgs",
           "[tcp_io] [lf_msg] [two_way] [interval_0] [many]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs),
                  true, 0, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

