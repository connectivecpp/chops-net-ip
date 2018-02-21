/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c udp_entity_io detail class.
 *
 *  This test design is different in a few respects from the tcp_io, tcp_acceptor,
 *  and tcp_connector tests. In particular, multiple UDP senders are sending to one
 *  UDP receiver, so an empty message shutdown sequence won't work the same as with
 *  TCP connections (which are always one-to-one).
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
#include <vector>
#include <functional> // std::ref, std::cref

#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/net_entity.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/component/worker.hpp"
#include "net_ip/component/send_to_all.hpp"

#include "net_ip/shared_utility_test.hpp"
#include "net_ip/shared_utility_func_test.hpp"

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

#include <iostream> // std::err for error sink

using namespace std::experimental::net;
using namespace chops::test;

// using notifier_cb = 
//   typename chops::net::detail::io_base<chops::net::detail::udp_io>::entity_notifier_cb;

const char*   test_addr = "127.0.0.1";
constexpr int test_port_base = 30665;
constexpr int NumMsgs = 50;

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

void udp_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_senders) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();
//  io_context ioc;
//  auto wg = make_work_guard(ioc);
//  std::thread thr( [&ioc] { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("UDP senders and a receiver are created") {
      THEN ("the futures provide synchronization") {

        auto recv_endp = make_udp_endpoint(test_addr, test_port_base);
        auto recv_ptr = std::make_shared<chops::net::detail::udp_entity_io>(ioc, recv_endp);

        INFO ("Receiving UDP entity created");

        chops::net::udp_err_wait_q err_wq;
        auto err_fut = std::async(std::launch::async,
          chops::net::ostream_error_sink_with_wait_queue<chops::net::udp_io>,
          std::ref(err_wq), std::ref(std::cerr));

        test_counter recv_cnt = 0;
        auto recv_io_futs = get_udp_io_futures(chops::net::udp_net_entity(recv_ptr), err_wq,
                                               reply, recv_cnt);

        auto recv_io = recv_io_futs.start_fut.get(); // UDP receiver is started

        chops::net::send_to_all<chops::net::udp_io> send_to_all_ios { };

        std::vector<chops::net::detail::udp_entity_io_ptr> senders;
        std::vector<chops::net::udp_io_interface_future> sender_fut_vec;

        test_counter send_cnt = 0;
        INFO ("Creating UDP senders, num: " << num_senders);
        chops::repeat(num_senders, [&] (int i) {
            ip::udp::endpoint send_endp = reply ? 
                ip::udp::endpoint(ip::udp::v4(), static_cast<unsigned short>(test_port_base + i + 1)) :
                ip::udp::endpoint();

            auto send_ptr = std::make_shared<chops::net::detail::udp_entity_io>(ioc, send_endp);
            senders.push_back(send_ptr);

            auto sender_futs = get_udp_io_futures(chops::net::udp_net_entity(send_ptr), err_wq,
                                                  false, send_cnt);
            auto send_io = sender_futs.start_fut.get();
            send_to_all_ios.add_io_interface(send_io);
            sender_fut_vec.emplace_back(std::move(sender_futs.stop_fut));
          }
        );
        // send messages through all of the senders
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        for (auto buf : in_msg_vec) {
          send_to_all_ios.send(buf);
          std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        // poll output queue size of all handlers until 0
        auto qs = send_to_all_ios.get_total_output_queue_stats();
        while (qs.output_queue_size > 0) {
std::cerr << "Output queue size: " << qs.output_queue_size << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          qs = send_to_all_ios.get_total_output_queue_stats();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // stop all handlers
        for (auto p : senders) {
          p->stop();
        }
        recv_ptr->stop();
        recv_io_futs.stop_fut.get();

        for (auto& fut : sender_fut_vec) {
          auto io = fut.get();
        }
        INFO ("All UDP entities stopped");

        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto err_cnt = err_fut.get();
        INFO ("Num err messages in sink: " << err_cnt);

        std::size_t total_msgs = num_senders * in_msg_vec.size();
        // CHECK instead of REQUIRE since UDP is an unreliable protocol
        CHECK (verify_receiver_count(total_msgs, recv_cnt));
        CHECK (verify_sender_count(total_msgs, send_cnt, reply));
      }
    }
  } // end given

//  wg.reset();
//  thr.join();
  wk.reset();
//  wk.stop();

}


SCENARIO ( "Udp IO test, checking flexibility in ipv4 vs ipv6 sending",
           "[udp_io] ") {

  auto ipv4_endp = make_udp_endpoint(test_addr, test_port_base);
  auto ipv6_endp = ip::udp::endpoint(ip::make_address("::1"), 
                                     static_cast<unsigned short>(test_port_base));

  auto ba = chops::make_byte_array(0x0D, 0x0E, 0x0A);

  GIVEN ("A UDP socket opened with ipv4") {

    io_context ioc;
    ip::udp::socket sock(ioc);
    sock.open(ip::udp::v4());
  
    INFO ("UDP socket opened");

    WHEN ("send_to is called with both ipv4 and ipv6 endpoints") {
      auto sz1 = sock.send_to(const_buffer(ba.data(), ba.size()), ipv4_endp);
//      auto sz2 = sock.send_to(const_buffer(ba.data(), ba.size()), ipv6_endp);
      THEN ("the call succeeds") {
        REQUIRE(sz1 == ba.size());
//        REQUIRE(sz2 == ba.size());
      }
    }
  } // end given

}

SCENARIO ( "Udp IO handler test, var len msgs, one-way, interval 50, senders 1",
           "[udp_io] [var_len_msg] [one_way] [interval_50] [senders_1]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
             false, 50, 1);

}

/*
SCENARIO ( "Udp IO handler test, var len msgs, one-way, interval 0, senders 1",
           "[udp_io] [var_len_msg] [one-way] [interval_0] [senders_1]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
             false, 0, 1);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 30, senders 10",
           "[udp_io] [var_len_msg] [two-way] [interval_30] [senders_10]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
             true, 30, 10);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 20, senders 5, many msgs",
           "[udp_io] [var_len_msg] [two_way] [interval_20] [senders_5] [many]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs),
             true, 20, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, one-way, interval 50, senders 5",
           "[udp_io] [cr_lf_msg] [one-way] [interval_50] [senders_5]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
             false, 50, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 20, senders 5",
           "[udp_io] [cr_lf_msg] [two-way] [interval_20] [senders_5]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
             true, 20, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 0, senders 1, many msgs",
           "[udp_io] [cr_lf_msg] [two_way] [interval_0] [senders_1] [many]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
             true, 0, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, one-way, interval 30, senders 1",
           "[udp_io] [lf_msg] [two-way] [interval_30] [senders_1]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
             false, 30, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 20, senders 10",
           "[udp_io] [lf_msg] [two-way] [interval_20] [senders_10]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
             true, 20, 10);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 50, senders 2, many msgs",
           "[udp_io] [lf_msg] [two-way] [interval_50] [senders_2] [many]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 30*NumMsgs),
             true, 50, 2);

}

*/

