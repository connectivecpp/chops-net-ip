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
#include "net_ip/component/send_to_all.hpp"
#include "net_ip/component/io_interface_delivery.hpp"
#include "net_ip/component/io_state_change.hpp"

#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/shared_utility_test.hpp"
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

void acc_conn_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
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

        test_counter acc_cnt = 0;
        acc_ptr->start(
          [reply, delim, &acc_cnt] (chops::net::tcp_io_interface io, std::size_t num, bool starting ) {
std::cerr << std::boolalpha << "acceptor state chg, starting flag: " << starting <<
", count: " << num << ", io state valid: " << io.is_valid() << std::endl;
            if (starting) {
              tcp_start_io(io, reply, delim, cnt);
            }
          },
          [] (chops::net::tcp_io_interface io, std::error_code err) {
std::cerr << std::boolalpha << "err func, err: " << err <<
", " << err.message() << ", io state valid: " << io.is_valid() << std::endl;
          }
        );

        REQUIRE(acc_ptr->is_started());

        chops::net::send_to_all<chops::net::tcp_io> sta { };

        std::vector<chops::net::detail::tcp_connector_ptr> connectors;
        std::vector<chops::net::tcp_io_interface_future> conn_fut_vec;

        test_counter conn_cnt = 0;
std::cerr << "creating " << num_conns << " connectors and futures" << std::endl;
        chops::repeat(num_conns, [&] () {

            auto conn_ptr = std::make_shared<chops::net::detail::tcp_connector>(ioc,
                           std::string_view(test_port), std::string_view(test_host),
                           std::chrono::milliseconds(ReconnTime));

            connectors.push_back(conn_ptr);

            auto conn_futs = 
              chops::net::make_tcp_io_interface_future_pair(chops::net::tcp_connector_net_entity(conn_ptr),
                                                            chops::net::make_



);
            auto conn_start_io = conn_futs.first.get();
            tcp_start_io(conn_start_io, false, delim, conn_cnt);
//            REQUIRE (conn_io.is_io_started());
            sta.add_io_interface(conn_start_io);
            conn_fut_vec.emplace_back(std::move(conn_futs.second));
          }
        );
        // send messages through all of the connectors
        for (auto buf : in_msg_vec) {
          sta.send(buf);
        }
        sta.send(empty_msg);

        for (auto& fut : conn_fut_vec) {
          auto io = fut.get();
        }

        acc_ptr->stop();
        INFO ("Acceptor stopped");
std::cerr << "Acceptor stopped" << std::endl;

//        REQUIRE_FALSE(acc_ptr->is_started());

        std::size_t total_msgs = num_conns * in_msg_vec.size();
        REQUIRE (verify_receiver_count(total_msgs, acc_cnt));
        REQUIRE (verify_sender_count(total_msgs, conn_cnt, reply));
  
      }
    }
  } // end given
  wk.reset();

}

SCENARIO ( "Tcp connector test, var len msgs, one-way, interval 50, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
                  false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

/*
SCENARIO ( "Tcp connector test, var len msgs, one-way, interval 0, 1 connector", 
           "[tcp_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
                  false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );
}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 50, 1 connector", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
                  true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
                  true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, var len msgs, two-way, interval 0, 60 connectors, many msgs", 
           "[tcp_conn] [var_len_msg] [two_way] [interval_0] [connectors_60] [many]" ) {

  acc_conn_test ( make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs),
                  true, 0, 60,
                  std::string_view(), make_empty_variable_len_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs),
                  false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
                  false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[tcp_conn] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
                  false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, two-way, interval 30, 20 connectors", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_30] [connectors_20]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs),
                  true, 30, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, CR / LF msgs, two-way, interval 0, 20 connectors, many msgs", 
           "[tcp_conn] [cr_lf_msg] [two_way] [interval_0] [connectors_20] [many]" ) {

  acc_conn_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
                  true, 0, 20, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, one-way, interval 50, 1 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
                  false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, one-way, interval 0, 25 connectors", 
           "[tcp_conn] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
                  false, 0, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, two-way, interval 20, 25 connectors", 
           "[tcp_conn] [lf_msg] [two_way] [interval_20] [connectors_25]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs),
                  true, 20, 25,
                  std::string_view("\n"), make_empty_lf_text_msg() );

}

SCENARIO ( "Tcp connector test, LF msgs, two-way, interval 0, 25 connectors, many msgs", 
           "[tcp_conn] [lf_msg] [two_way] [interval_0] [connectors_25] [many]" ) {

  acc_conn_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs),
                  true, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );

}
*/
