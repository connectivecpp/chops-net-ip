/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_ip class.
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


#include "net_ip/net_ip.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip/component/worker.hpp"
#include "net_ip/component/send_to_all.hpp"

#include "net_ip/shared_utility_test.hpp"
#include "net_ip/shared_utility_func_test.hpp"

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"

#include <iostream> // std::cerr for error sink

using namespace std::experimental::net;
using namespace chops::test;

const char* tcp_test_port = "30465";
const char* tcp_test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 500;

const char*   udp_test_addr = "127.0.0.1";
constexpr int udp_port_base = 31445;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

ip::udp::endpoint make_test_udp_endpoint(int udp_port_num) {
  return ip::udp::endpoint(ip::make_address(udp_test_addr),
                           static_cast<unsigned short>(udp_port_num));
}

void acc_conn_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("when done, the counts all match") {

        chops::net::net_ip nip(ioc);

        auto acc = nip.make_tcp_acceptor(tcp_test_port, tcp_test_host);
//        REQUIRE_FALSE(acc.is_started());

        test_counter acc_cnt = 0;
        acc.start( [reply, delim, &acc_cnt] (chops::net::tcp_io_interface io, std::size_t) {
            tcp_start_io(io, reply, delim, acc_cnt);
          }
        );
std::cerr << "acceptor created and started, start_io will be called" << std::endl;

        REQUIRE(acc.is_started());

        chops::net::send_to_all<chops::net::tcp_io> sta { };

        std::vector< chops::net::tcp_connector_net_entity > connectors;
        std::vector< std::future<chops::net::tcp_io_interface> > conn_fut_vec;

        test_counter conn_cnt = 0;
std::cerr << "creating " << num_conns << " connectors" << std::endl;
        chops::repeat(num_conns, [&] () {

            auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host,
                                               std::chrono::milliseconds(ReconnTime));
            connectors.push_back(conn);

            auto conn_futs = 
              chops::net::make_tcp_io_interface_future_pair(conn);
            auto conn_start_io = conn_futs.first.get();
            tcp_start_io(conn_start_io, false, delim, conn_cnt);
//            REQUIRE (conn_io.is_io_started());
            sta.add_io_interface(conn_start_io);
            conn_fut_vec.emplace_back(std::move(conn_futs.second));
          }
        );

        for (auto buf : in_msg_vec) {
          sta.send(buf);
        }
        sta.send(empty_msg);

        for (auto& fut : conn_fut_vec) {
          auto io = fut.get();
        }

        acc.stop();
        nip.remove(acc);

        nip.stop_all();
        nip.remove_all();
    
        std::size_t total_msgs = num_conns * in_msg_vec.size();
        REQUIRE (verify_receiver_count(total_msgs, acc_cnt));
        REQUIRE (verify_sender_count(total_msgs, conn_cnt, reply));
      }
    }
  } // end given
  wk.reset();
}

void udp_test (const vec_buf& in_msg_vec, int interval, int num_udp_pairs, 
               chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("pairs of udp senders and receivers are created") {
      THEN ("when done, the counts all match") {

        chops::net::net_ip nip(ioc);

std::cerr << "creating " << num_udp_pairs << " udp sender receiver pairs" << std::endl;

        chops::net::send_to_all<chops::net::udp_io> sta { };

        test_counter recv_cnt = 0;
        test_counter send_cnt = 0;

        chops::repeat(num_udp_pairs, [&] (int i) {
            auto recv_endp = make_test_udp_endpoint(udp_port_base + i);

            auto udp_receiver = nip.make_udp_unicast(recv_endp);
            auto udp_sender = nip.make_udp_sender();

            auto io_fut = chops::net::make_udp_io_interface_future(udp_receiver);
            auto io = io_fut.get();
            udp_start_io(io, false, recv_cnt);

            io_fut = chops::net::make_udp_io_interface_future(udp_sender);
            io = io_fut.get();
            udp_start_io(io, false, send_cnt, recv_endp);

            sta.add_io_interface(io);

          }
        );

        // send messages through all of the senders
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        for (auto buf : in_msg_vec) {
          sta.send(buf);
          std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        // poll output queue size of all handlers until 0
        auto qs = sta.get_total_output_queue_stats();
        while (qs.output_queue_size > 0) {
std::cerr << "Output queue size: " << qs.output_queue_size << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          qs = sta.get_total_output_queue_stats();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

        nip.stop_all();
std::cerr << "Udp entities stopped" << std::endl;
        nip.remove_all();


        std::size_t total_msgs = num_udp_pairs * in_msg_vec.size();
        // CHECK instead of REQUIRE since UDP is an unreliable protocol
        CHECK (verify_receiver_count(total_msgs, recv_cnt));
        CHECK (verify_sender_count(total_msgs, send_cnt, false));

      }
    }
  } // end given
  wk.stop();
}


SCENARIO ( "Net IP test, var len msgs, one-way, interval 50, 1 connector or pair", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 50, 1,
             make_empty_variable_len_msg() );

}

/*
SCENARIO ( "Net IP test, var len msgs, one-way, interval 0, 1 connector or pair", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  acc_conn_test ( ms, false, 0, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 1,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 50, 1 connector or pair", 
           "[net_ip] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  acc_conn_test ( ms, true, 50, 1,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 50, 1,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
           "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  acc_conn_test ( ms, true, 0, 10,
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 10,
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, var len msgs, two-way, interval 0, 40 connectors or pairs", 
           "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  auto ms = make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs);
  acc_conn_test ( ms, true, 0, 40, 
                  std::string_view(), make_empty_variable_len_msg() );
  udp_test ( ms, 0, 40, 
             make_empty_variable_len_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 50, 1 connector or pair", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 50, 1,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 50, 10 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  acc_conn_test ( ms, false, 50, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 50, 10,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, one-way, interval 0, 20 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  acc_conn_test ( ms, false, 0, 20,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 0, 20,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, two-way, interval 30, 10 connectors or pairs", 
           "[net_ip] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  acc_conn_test ( ms, true, 30, 10,
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 30, 10,
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test, CR / LF msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
           "[net_ip] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  acc_conn_test ( ms, true, 0, 10, 
                  std::string_view("\r\n"), make_empty_cr_lf_text_msg() );
  udp_test ( ms, 0, 10, 
             make_empty_cr_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, one-way, interval 50, 1 connector or pair", 
           "[net_ip] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  acc_conn_test ( ms, false, 50, 1,
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 50, 1,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, one-way, interval 0, 25 connectors or pairs", 
           "[net_ip] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  acc_conn_test ( ms, false, 0, 25, 
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 0, 25,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, two-way, interval 20, 15 connectors or pairs", 
           "[net_ip] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  acc_conn_test ( ms, true, 20, 15, 
                  std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 20, 15,
             make_empty_lf_text_msg() );

}

SCENARIO ( "Net IP test,  LF msgs, two-way, interval 0, 15 connectors or pairs, many msgs", 
           "[net_ip] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  auto ms = make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  acc_conn_test ( ms, true, 0, 15, 
             std::string_view("\n"), make_empty_lf_text_msg() );
  udp_test ( ms, 0, 15, 
             make_empty_lf_text_msg() );

}
*/
