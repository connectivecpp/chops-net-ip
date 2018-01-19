/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_acceptor detail class.
 *
 *  This test is similar to the tcp_io_test code, without all of the 
 *  internal plumbing needed, and allowing multiple connector threads to
 *  be started. The TCP acceptor is the Chops Net IP class, but the 
 *  connector threads are using blocking Networking TS connects and io.
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
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>

#include "net_ip/detail/tcp_acceptor.hpp"

#include "net_ip/worker.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"


using namespace std::experimental::net;
using namespace chops::test;

const char* test_port = "30434";
constexpr int NumMsgs = 50;


// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

void connector_func (std::promise<std::size_t> thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                     bool read_reply, int interval, chops::const_shared_buffer empty_msg) {

  ip::tcp::socket sock(ioc);
  auto endp_seq = chops::net::endpoints_resolver(ioc).make_endpoints(true, "", test_port);
  ip::tcp::endpoint endp = connect(sock, endp_seq);

  std::size_t cnt = 0;
  chops::mutable_shared_buffer return_msg { };
  for (auto buf : in_msg_set) {
    write(const_buffer(buf.data(), buf.size()));
    if (read_reply) {
      return_msg.resize(buf.size());
      read(mutable_buffer(return_msg.data(), return_msg.size()));
      ++cnt;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  // shutdown message flow - send empty msg, receive empty msg, send it again, causes
  // msg handler to close connection on other side
  write(const_buffer(empty_msg.data(), empty_msg.size()));
  return_msg.resize(empty_msg.size());
  read(mutable_buffer(return_msg.data(), return_msg.size()));
  write(const_buffer(empty_msg.data(), empty_msg.size()));

  thr_prom.set_value(cnt);

}

struct start_cb {
  vec_buf                              vb;
  std::size_t                          num;
  msg_hdlr<chops::net::detail::tcp_io> mh;
  std::string_view                     delim;


  start_cb (bool reply, std::string_view d) : vb(), num(0), mh(vb, reply), delim(d) { }

  void operator() (tcp_io_interface io, std::size_t n) {
    num = n;
    if (delim.empty()) {
      io.start_io(mh, chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr), 2);
    }
    else {
      io.start_io(mh, delim);
    }
  }

};

struct shut_cb {

  std::size_t         num;
  std::error_code     err;
  void operator() (tcp_io_interface io, std::error_code e, std::size_t n) {
    num = n;
    err = e;
  }

};


void acceptor_test (const vec_buf& in_msg_set, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("the futures provide synchronization and data returns") {

        auto endp_seq = 
          chops::net::endpoints_resolver(wk.get_io_context()).make_endpoints(true, "", test_port);
        chops::net::detail::tcp_acceptor acc(wk.get_io_context(), *(endp_seq.cbegin()), true);

        REQUIRE_FALSE(acc.is_started());

        start_cb start_callback(reply, delim);
        shut_cb  shut_callback { };

        acc.start(std::ref(start_callback), std::ref(shut_callback));

        REQUIRE(acc.is_started());
        REQUIRE(start_callback.num = 0);
        REQUIRE(start_callback.vb.empty());

        std::vector<std::thread> conn_thrs;
        std::vector<std::future<std::size_t> > conn_futs;

        chops::repeat(num_conns, [&] () {
            std::promise<std::size_t> conn_prom;
            conn_futs.emplace_back(conn_prom.get_future());
            conn_thrs.emplace_back(connector_func, std::move(conn_prom), std::cref(in_msg_set), 
                                   std::ref(wk.get_io_context()), reply, interval, empty_msg);
          }
        );

        std::size_t accum_msgs = 0;

        for (auto& fut : conn_futs) {
          accum_msgs += fut.get();
        }
        INFO ("Connector futures popped");

        for (auto& thr : conn_thrs) {
          thr.join();
        }
        INFO ("Connector threads joined");

        acc.stop();

        INFO ("Acceptor stopped");

        REQUIRE_FALSE(acc.is_started());

        REQUIRE(start_callback.num = num_conns);
        REQUIRE(shut_callback.num = 0);
        REQUIRE(shut_callback.err);
        INFO ("Last shutdown callback error code and msg: " << shut_callback.err << " " 
                                                            << shut_callback.err.message() );
        std::size_t total_msgs = num_msgs * in_msg_set.size();
        REQUIRE (accum_msgs == total_msgs);
        REQUIRE (start_callback.vb.size() == total_msgs);
      }
    }
  } // end given
  wk.stop();

}

void acceptor_test (const vec_buf& in_msg_set, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

SCENARIO ( "Tcp acceptor test, variable len msgs, interval 50, 1 connector, one-way", "[tcp_acc_one_way_var]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acceptor_test ( ms, false, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, variable len msgs, interval 0, one-way", "[tcp_acc_one_way_var_fast]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acceptor_test ( ms, false, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, variable len msgs, interval 50, two-way", "[tcp_acc_two_way_var]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acceptor_test ( ms, true, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, variable len msgs, interval 0, two-way, many msgs", "[tcp_acc_two_way_var_fast_many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acceptor_test ( ms, true, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 50, one-way", "[tcp_acc_one_way_cr_lf]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acceptor_test ( ms, false, 50, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 0, one-way", "[tcp_acc_one_way_cr_lf_fast]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acceptor_test ( ms, false, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 30, two-way", "[tcp_acc_two_way_cr_lf]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acceptor_test ( ms, true, 30, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, interval 0, two-way, many msgs", "[tcp_acc_two_way_cr_lf_fast_many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acceptor_test ( ms, true, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, LF msgs, interval 50, one-way", "[tcp_acc_one_way_lf]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acceptor_test ( ms, false, 50, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, LF msgs, interval 0, one-way", "[tcp_acc_one_way_lf_fast]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acceptor_test ( ms, false, 0, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, LF msgs, interval 20, two-way", "[tcp_acc_two_way_lf]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acceptor_test ( ms, true, 20, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Tcp acceptor test, LF msgs, interval 0, two-way, many msgs", "[tcp_acc_two_way_lf_fast_many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acceptor_test ( ms, true, 0, std::string_view("\n"), empty_msg );

}
