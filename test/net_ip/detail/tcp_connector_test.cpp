/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_connector detail class.
 *
 *  This test is similar to the tcp_acceptor_test code, except the Chops Net IP 
 *  tcp_connector class instead of Networking TS blocking io calls.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/buffer>
#include <experimental/io_context>

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move, std::ref
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"

#include "net_ip/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"


// #include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

const char* test_port = "30565";
const char* test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 100;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

void start_io (chops::net::tcp_io_interface io, bool reply, std::string_view delim) {
  if (delim.empty()) {
    io.start_io(2, msg_hdlr<chops::net::detail::tcp_io>(reply), 
                chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr));
  }
  else {
    io.start_io(delim, msg_hdlr<chops::net::detail::tcp_io>(reply));
  }
}

struct acc_start_cb {
  std::string_view     delim;
  bool                 reply;

  acc_start_cb (bool r, std::string_view d) : delim(d), reply(r) { }

  void operator() (chops::net::tcp_io_interface io, std::size_t n) const {
// std::cerr << "Acceptor start_cb invoked, num hdlrs: " << n 
// << std::boolalpha << ", io is_valid: " << io.is_valid() << std::endl;
    start_io(io, reply, delim);
  }

};

void acc_shut_cb(chops::net::tcp_io_interface io, std::error_code e, std::size_t n) {

// std::cerr << "Acceptor shut_cb invoked, num hdlrs: " << n 
// << std::boolalpha << ", err: " << e << ", " << e.message() << ", io is_valid: " 
// << io.is_valid() << std::endl;

}

using tcp_io_promise = std::promise<chops::net::tcp_io_interface>;

struct conn_cb {
  tcp_io_promise       io_prom;
  std::string_view     delim;

  conn_cb (tcp_io_promise iop, std::string_view d) : 
    io_prom(std::move(iop)), delim(d) { }

  void operator() (chops::net::tcp_io_interface io, std::size_t n) {

// std::cerr << "Connector start_cb invoked, num hdlrs: " << n 
// << std::boolalpha << ", io is_valid: " << io.is_valid() << std::endl;

    start_io(io, false, delim);
    io_prom.set_value(io);
  }

  void operator() (chops::net::tcp_io_interface io, std::error_code e, std::size_t n) {
// std::cerr << "Connector shut cb invoked, num hdlrs: " << n 
// << std::boolalpha << ", io is_valid: " << io.is_valid() 
// << ", err: " << e << ", " << e.message() << std::endl;
  }

};


std::size_t connector_func (const vec_buf& in_msg_set, io_context& ioc, 
                            bool read_reply, int interval, std::string_view delim,
                            chops::const_shared_buffer empty_msg) {

  auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc, 
                           std::string_view(test_port), std::string_view(test_host),
                           std::chrono::milliseconds(ReconnTime));

  tcp_io_promise io_prom;
  auto io_fut = io_prom.get_future();
  conn_cb cb(std::move(io_prom), delim);

  conn_ptr->start(std::ref(cb), std::ref(cb));

  auto io = io_fut.get();

  std::size_t cnt = 0;
  for (auto buf : in_msg_set) {
    io.send(std::move(buf));
    ++cnt;
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
// if (cnt % 100 == 0) {
//  auto qs = io.get_output_queue_stats();
//  std::cerr << "Send loop at cnt: " << cnt << ", stats: " << qs.output_queue_size
//  << ", " << qs.bytes_in_output_queue << std::endl;
// }
  }
  io.send(empty_msg);
// std::cerr << "Empty message sent" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  conn_ptr->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  return cnt;
}

void acc_conn_test (const vec_buf& in_msg_set, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endp_seq = 
            chops::net::endpoints_resolver<ip::tcp>(ioc).make_endpoints(true, test_host, test_port);
        auto acc_ptr = 
            std::make_shared<chops::net::detail::tcp_acceptor>(ioc, *(endp_seq.cbegin()), true);

// std::cerr << "acceptor created" << std::endl;

        REQUIRE_FALSE(acc_ptr->is_started());

        acc_ptr->start(acc_start_cb(reply, delim), acc_shut_cb);

        REQUIRE(acc_ptr->is_started());

        std::vector<std::future<std::size_t> > conn_futs;

// std::cerr << "creating " << num_conns << " async futures and threads" << std::endl;
        chops::repeat(num_conns, [&] () {
            conn_futs.emplace_back(std::async(std::launch::async, connector_func, std::cref(in_msg_set), 
                                   std::ref(ioc), reply, interval, delim, empty_msg));

          }
        );

        std::size_t accum_msgs = 0;
        for (auto& fut : conn_futs) {
          accum_msgs += fut.get(); // wait for connectors to finish
        }
        INFO ("All connector futures popped");
// std::cerr << "All connector futures popped" << std::endl;

        acc_ptr->stop();

        INFO ("Acceptor stopped");
// std::cerr << "Acceptor stopped" << std::endl;

        REQUIRE_FALSE(acc_ptr->is_started());

        std::size_t total_msgs = num_conns * in_msg_set.size();
        REQUIRE (accum_msgs == total_msgs);
      }
    }
  } // end given
  wk.stop();

}

SCENARIO ( "Tcp acceptor test, var len msgs, interval 50, 1 connector, one-way", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, var len msgs, interval 0, 1 connector, one-way", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 0, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, var len msgs, interval 50, 1 connector, two-way", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 50, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, var len msgs, interval 0, 10 connectors, two-way, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, 10, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, var len msgs, interval 0, 60 connectors, two-way, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_60] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, 60, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 50, 1 connectors, one-way", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 50, 10 connectors, one-way", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, 10, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 0, 20 connectors, one-way", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 0, 20, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 30, 20 connectors, two-way", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 30, 20, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 0, 20 connectors, two-way, many msgs", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_0] [connectors_20] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 0, 20, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, interval 50, 1 connectors, one-way", 
           "[tcp_conn] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, interval 0, 25 connectors, one-way", 
           "[tcp_conn] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 0, 25, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, interval 20, 25 connectors, two-way", 
           "[tcp_conn] [lf_msg] [two_way] [interval_20] [connectors_25]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 20, 25, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, interval 0, 25 connectors, two-way, many msgs", 
           "[tcp_conn] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 0, 25, std::string_view("\n"), empty_msg );

}
