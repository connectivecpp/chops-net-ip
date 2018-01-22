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
#include <functional>
#include <string_view>

#include "net_ip/detail/tcp_io.hpp"

#include "net_ip/worker.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"


#include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

using notifier_cb = 
  typename chops::net::detail::io_base<chops::net::detail::tcp_io>::entity_notifier_cb;

constexpr int test_port = 30434;
const char*   test_addr = "0.0.0.0";
constexpr int NumMsgs = 50;


void notify_me(std::error_code e, chops::net::detail::tcp_io_ptr p) {
  std::cerr << "Inside notify_me, err: " << e << ", " << e.message() << std::endl;
  p->close();
}

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;
using prom_type = std::promise<std::size_t>;

auto invoke_start_io(chops::net::detail::tcp_io_ptr iohp, bool reply, std::string_view delim) {

  prom_type mh_prom;
  auto mh_fut = mh_prom.get_future();
  if (delim.empty()) {
    iohp->start_io(msg_hdlr<chops::net::detail::tcp_io>(reply, std::move(mh_prom)), 
                   chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr), 2);
  }
  else {
    iohp->start_io(msg_hdlr<chops::net::detail::tcp_io>(reply, std::move(mh_prom)), delim);
  }
  return mh_fut;
}

std::size_t connector_func (const vec_buf& in_msg_set, io_context& ioc, 
                     int interval, std::string_view delim, chops::const_shared_buffer empty_msg) {
  using namespace std::placeholders;

  ip::tcp::socket sock(ioc);
  sock.connect(ip::tcp::endpoint(ip::make_address(test_addr), test_port));

  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), notify_me);
  auto mh_fut = invoke_start_io(iohp, false, delim);

  for (auto buf : in_msg_set) {
    iohp->send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  return mh_fut.get();
}

void acc_conn_test (const vec_buf& in_msg_set, bool reply, int interval, std::string_view delim,
                    chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and connector are created") {
      THEN ("the futures provide synchronization and data returns") {

        ip::tcp::acceptor acc(wk.get_io_context(), ip::tcp::endpoint(ip::address_v4::any(), test_port));

        INFO ("Creating connector asynchronously, msg interval: " << interval);

        auto conn_fut = std::async(std::launch::async, connector_func, std::cref(in_msg_set), 
                                   std::ref(wk.get_io_context()), interval, delim, empty_msg);

        auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(acc.accept()), notify_me);
        auto mh_fut = invoke_start_io(iohp, reply, delim);

        auto acc_cnt = mh_fut.get(); // wait for mh termination callback
        INFO ("Msg hdlr future popped");

        auto conn_cnt = conn_fut.get(); // get msg set size from connector
        INFO ("Connector future popped");
        REQUIRE (in_msg_set.size() == acc_cnt);
        if (reply) {
          REQUIRE (in_msg_set.size() == conn_cnt);
        }
        else {
          REQUIRE (conn_cnt == 0);
        }
      }
    }
  } // end given
  wk.reset();

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

