/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>

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

#include "net_ip/component/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"

#include "net_ip/component/simple_variable_len_msg_frame.hpp"

// #include <iostream>

using namespace std::experimental::net;
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


void invoke_start_io(chops::net::detail::tcp_io_ptr iohp, bool reply, std::string_view delim) {
  if (delim.empty()) {
    iohp->start_io(2, msg_hdlr<chops::net::detail::tcp_io>(reply), 
                   chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr));
  }
  else {
    iohp->start_io(delim, msg_hdlr<chops::net::detail::tcp_io>(reply));
  }
}

std::error_code connector_func (const vec_buf& in_msg_set, io_context& ioc, 
                     int interval, std::string_view delim, chops::const_shared_buffer empty_msg) {
  using namespace std::placeholders;

  auto endps = 
      chops::net::endpoints_resolver<ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
  ip::tcp::socket sock(ioc);
  connect(sock, endps);

  notify_prom_type notify_prom;
  auto notify_fut = notify_prom.get_future();

  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), 
                                                           notify_me(std::move(notify_prom)));
  invoke_start_io(iohp, false, delim);

  for (auto buf : in_msg_set) {
    iohp->send(buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  return notify_fut.get();
}

void acc_conn_test (const vec_buf& in_msg_set, bool reply, int interval, std::string_view delim,
                    chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();
//  io_context ioc;
//  auto wg = make_work_guard(ioc);
//  std::thread thr( [&ioc] { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and connector are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endps = 
            chops::net::endpoints_resolver<ip::tcp>(ioc).make_endpoints(true, test_addr, test_port);
        ip::tcp::acceptor acc(ioc, *(endps.cbegin()));

        INFO ("Creating connector asynchronously, msg interval: " << interval);

        auto conn_fut = std::async(std::launch::async, connector_func, std::cref(in_msg_set), 
                                   std::ref(ioc), interval, delim, empty_msg);

        notify_prom_type notify_prom;
        auto notify_fut = notify_prom.get_future();

        auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(acc.accept()), 
                                                                 notify_me(std::move(notify_prom)));
        invoke_start_io(iohp, reply, delim);

        auto acc_err = notify_fut.get();
// std::cerr << "Inside acc_conn_test, acc_err: " << acc_err << ", " << acc_err.message() << std::endl;
        auto conn_err = conn_fut.get(); // get std::error_code from connector
// std::cerr << "Inside acc_conn_test, connector_func future popped, val: " << conn_err << std::endl;

        REQUIRE (true);
      }
    }
  } // end given

//  wg.reset();
//  thr.join();
  wk.reset();
//  wk.stop();

}


SCENARIO ( "Tcp IO handler test, variable len msgs, interval 50, one-way",
           "[tcp_io] [one_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, interval 0, one-way",
           "[tcp_io] [one_way] [var_len_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, interval 50, two-way",
           "[tcp_io] [two_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp IO handler test, variable len msgs, interval 0, two-way, many msgs",
           "[tcp_io] [two_way] [var_len_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, interval 50, one-way",
           "[tcp_io] [one_way] [cr_lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, interval 0, one-way",
           "[tcp_io] [one_way] [cr_lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, interval 30, two-way",
           "[tcp_io] [two_way] [cr_lf_msg] [interval_30]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 30, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, CR / LF msgs, interval 0, two-way, many msgs",
           "[tcp_io] [two_way] [cr_lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, LF msgs, interval 50, one-way",
           "[tcp_io] [one_way] [lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, LF msgs, interval 0, one-way",
           "[tcp_io] [one_way] [lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, LF msgs, interval 20, two-way",
           "[tcp_io] [two_way] [lf_msg] [interval_20]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 20, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp IO handler test, LF msgs, interval 0, two-way, many msgs",
           "[tcp_io] [two_way] [lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\n"), empty_msg );

}

