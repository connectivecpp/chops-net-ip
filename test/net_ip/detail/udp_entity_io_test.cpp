/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c udp_entity_io detail class.
 *
 *  @author Cliff Green
 *  @date 2018
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

#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"


// #include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

// using notifier_cb = 
//   typename chops::net::detail::io_base<chops::net::detail::udp_io>::entity_notifier_cb;

const char*   test_port = "30665";
const char*   test_addr = "";
constexpr int NumMsgs = 50;
constexpr int MaxSize = 65507;

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

struct state_chg_cb {

  bool started = false;
  std::error_code last_err = std::error_code();
  std::size_t     last_sz = 0;

  void operator()(chops::net::detail::udp_io p, std::size_t sz) {
    started = true;
    last_sz = sz;
  }

  void operator()(chops::net::detail::udp_io p, std::error_code e, std::size_t sz) {
    last_err = e;
    last_sz = sz;
  }

};


void invoke_start_io(chops::net::detail::udp_entity_io_ptr iohp, bool reply, bool receive,
                     ip::udp::endpoint endp) {
  if (receive) {
    iohp->start_io(MaxSize, msg_hdlr<chops::net::detail::udp_io>(reply));
  }
  else {
    iohp->start_io(endp);
  }
}

std::error_code sender_func (const vec_buf& in_msg_set, io_context& ioc, bool receive,
                             int interval, chops::const_shared_buffer empty_msg) {

  auto endps = chops::net::endpoints_resolver<ip::udp>(ioc).make_endpoints(true, test_addr, test_port);
  ip::udp::endpoint dest_endp = endps.cbegin()->endpoint();

  auto iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, ip::udp::endpoint());
  state_chg_cb cb;

  iohp->start(std::ref(cb), std::ref(cb));
  invoke_start_io(iohp, false, receive, dest_endp);

  for (auto buf : in_msg_set) {
    iohp->send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  return in_msg_set.size();
}

void udp_test (const vec_buf& in_msg_set, bool reply, int interval, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();
//  io_context ioc;
//  auto wg = make_work_guard(ioc);
//  std::thread thr( [&ioc] { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("a UDP sender and receiver are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endps = 
            chops::net::endpoints_resolver<ip::udp>(ioc).make_endpoints(true, test_addr, test_port);
        ip::udp::acceptor acc(ioc, *(endps.cbegin()));

        INFO ("Creating connector asynchronously, msg interval: " << interval);

        auto conn_fut = std::async(std::launch::async, connector_func, std::cref(in_msg_set), 
                                   std::ref(ioc), interval, delim, empty_msg);

        notify_prom_type notify_prom;
        auto notify_fut = notify_prom.get_future();

        auto iohp = std::make_shared<chops::net::detail::udp_io>(std::move(acc.accept()), 
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


SCENARIO ( "Udp IO handler test, variable len msgs, interval 50, one-way",
           "[udp_io] [one_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 0, one-way",
           "[udp_io] [one_way] [var_len_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 50, two-way",
           "[udp_io] [two_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [var_len_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 50, one-way",
           "[udp_io] [one_way] [cr_lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 0, one-way",
           "[udp_io] [one_way] [cr_lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 30, two-way",
           "[udp_io] [two_way] [cr_lf_msg] [interval_30]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 30, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [cr_lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 50, one-way",
           "[udp_io] [one_way] [lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 0, one-way",
           "[udp_io] [one_way] [lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 20, two-way",
           "[udp_io] [two_way] [lf_msg] [interval_20]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 20, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\n"), empty_msg );

}

