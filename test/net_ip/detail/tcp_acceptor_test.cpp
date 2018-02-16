/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_acceptor detail class.
 *
 *  This test is similar to the tcp_io_test code, with a little bit less
 *  internal plumbing, and allowing multiple connector threads to be started. 
 *  The TCP acceptor is the Chops Net IP class, but the connector threads are 
 *  using blocking Networking TS connects and io.
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

#include "net_ip/component/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"


// #include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

const char* test_port = "30434";
const char* test_host = "";
constexpr int NumMsgs = 50;


// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

std::size_t connector_func (const vec_buf& in_msg_vec, io_context& ioc, 
                            bool read_reply, int interval, chops::const_shared_buffer empty_msg) {

  ip::tcp::socket sock(ioc);
  auto endp_seq = chops::net::endpoints_resolver<ip::tcp>(ioc).make_endpoints(true, test_host, test_port);
  ip::tcp::endpoint endp = connect(sock, endp_seq);

  std::size_t cnt = 0;
  chops::mutable_shared_buffer return_msg { };
  for (auto buf : in_msg_vec) {
    write(sock, const_buffer(buf.data(), buf.size()));
    if (read_reply) {
      return_msg.resize(buf.size());
      read(sock, mutable_buffer(return_msg.data(), return_msg.size()));
      ++cnt;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  write(sock, const_buffer(empty_msg.data(), empty_msg.size()));
  char c;
  std::error_code ec;
  auto sz = read(sock, mutable_buffer(&c, 1), ec); // block on read until connection is closed
// std::cerr << "Single byte read completed, sz: " << sz << ", err: " << ec << ", " 
// << ec.message() << std::endl;
  return cnt;
}

void acceptor_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
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

        test_counter cnt = 0;
        acc_ptr->start( [reply, delim, &cnt] (chops::net::tcp_io_interface io, std::size_t /* num */) {
            tcp_start_io(io, reply, delim, cnt);
          }
        );

// std::cerr << "acceptor started" << std::endl;

        REQUIRE(acc_ptr->is_started());

        std::vector<std::future<std::size_t> > conn_futs;

// std::cerr << "creating " << num_conns << " async futures and threads" << std::endl;
        chops::repeat(num_conns, [&] () {
            conn_futs.emplace_back(std::async(std::launch::async, connector_func, std::cref(in_msg_vec), 
                                   std::ref(ioc), reply, interval, empty_msg));

          }
        );

        std::size_t conn_cnt = 0;
        for (auto& fut : conn_futs) {
          conn_cnt += fut.get(); // wait for connectors to finish
        }
        INFO ("All connector futures popped");
// std::cerr << "All connector futures popped" << std::endl;

        acc_ptr->stop();

        INFO ("Acceptor stopped");
// std::cerr << "Acceptor stopped" << std::endl;

        REQUIRE_FALSE(acc_ptr->is_started());

        std::size_t total_msgs = num_conns * in_msg_vec.size();
        REQUIRE (verify_receiver_count(total_msgs, cnt));
        REQUIRE (verify_sender_count(total_msgs, conn_cnt, reply));
      }
    }
  } // end given
  wk.reset();

}

SCENARIO ( "Tcp acceptor test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                  false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_acc] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                  false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 50, 1 connector", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                  true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                  true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, var len msgs, two-way, interval 0, 60 connectors, many msgs", 
           "[tcp_acc] [var_len_msg] [two_way] [interval_0] [connectors_60] [many]" ) {

  acceptor_test ( make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 50*NumMsgs),
                  true, 0, 60, 
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Whaaaat", 'T', NumMsgs),
                  false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                  false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                  false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, two-way, interval 30, 20 connectors", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                  true, 30, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test, CR / LF msgs, two-way, interval 0, 20 connectors, many msgs", 
           "[tcp_acc] [cr_lf_msg] [two_way] [interval_0] [connectors_20] [many]" ) {

  acceptor_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 50*NumMsgs),
                  true, 0, 20, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                  false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, one-way, interval 0, 25 connectors", 
           "[tcp_acc] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                  false, 0, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, two-way, interval 20, 25 connectors", 
           "[tcp_acc] [lf_msg] [two_way] [interval_20] [connectors_25]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs),
                  true, 20, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp acceptor test,  LF msgs, two-way, interval 0, 25 connectors, many msgs", 
           "[tcp_acc] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  acceptor_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 80*NumMsgs),
                  true, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}
