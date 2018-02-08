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

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"

#include "net_ip/component/worker.hpp"
#include "net_ip/component/simple_variable_len_msg_frame.hpp"
#include "net_ip/component/send_to_all.hpp"
#include "net_ip/component/io_interface_future.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"


#include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

const char* test_port = "30777";
const char* test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 100;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

using mh_type = msg_hdlr<chops::net::detail::tcp_io>;

void start_io (chops::net::tcp_io_interface io, bool reply, 
               std::string_view delim, mh_type& mh) {
  if (delim.empty()) {
    io.start_io(2, mh, chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr));
  }
  else {
    io.start_io(delim, mh);
  }
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

std::cerr << "acceptor created" << std::endl;

//        REQUIRE_FALSE(acc_ptr->is_started());

        mh_type acc_mh(reply);

        acc_ptr->start( [reply, delim, &acc_mh] 
              (chops::net::tcp_io_interface io, std::size_t /* num */) mutable {
            start_io(io, reply, delim, acc_mh);
          }
        );

//        REQUIRE(acc_ptr->is_started());

        chops::net::send_to_all<chops::net::tcp_io> sta { };

        mh_type conn_mh(false);
        std::vector<chops::net::detail::tcp_connector_ptr> connectors;

std::cerr << "creating " << num_conns << " connectors and futures" << std::endl;
        chops::repeat(num_conns, [&] () {

            auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                           std::string_view(test_port), std::string_view(test_host),
                           std::chrono::milliseconds(ReconnTime));
//            REQUIRE_FALSE (conn_ptr->is_started());
//            REQUIRE_FALSE (conn_ptr->is_io_started());

            connectors.push_back(conn_ptr);

            auto conn_fut = 
              chops::net::make_tcp_io_interface_future(chops::net::tcp_connector_net_entity(conn_ptr));
            auto conn_io = conn_fut.get();
            start_io(conn_io, false, delim, conn_mh);
//            REQUIRE (conn_io.is_io_started());
            sta.add_io_interface(conn_io);
          }
        );
        // send messages through all of the connectors
        for (auto buf : in_msg_set) {
          sta.send(buf);
        }
        sta.send(empty_msg);

        acc_ptr->stop();
        INFO ("Acceptor stopped");
std::cerr << "Acceptor stopped" << std::endl;

/*
        REQUIRE_FALSE(acc_ptr->is_started());

        std::size_t total_msgs = num_conns * in_msg_set.size();
        REQUIRE (acc_mh.msgs.size() == total_msgs);
        if (reply) {
          REQUIRE (conn_mh.msgs.size() == total_msgs);
        }
        else {
          REQUIRE (conn_mh.msgs.size() == 0);
        }
*/
      }
    }
  } // end given
  wk.reset();

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
