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
#include "net_ip/component/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/component/io_interface_future.hpp"
#include "net_ip/component/send_to_all.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"

#include "utility/shared_buffer.hpp"
#include "utility/make_byte_array.hpp"


// #include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

// using notifier_cb = 
//   typename chops::net::detail::io_base<chops::net::detail::udp_io>::entity_notifier_cb;

const char*   test_addr = "127.0.0.1";
constexpr int test_port_base = 30665;
constexpr int NumMsgs = 50;
constexpr int MaxSize = 65507;

ip::udp::endpoint make_test_endpoint(int port_num) {
  return ip::udp::endpoint(ip::make_address(test_addr),
                           static_cast<unsigned short>(port_num));
}

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

void udp_test (const vec_buf& in_msg_set, bool reply, int interval, int num_senders) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();
//  io_context ioc;
//  auto wg = make_work_guard(ioc);
//  std::thread thr( [&ioc] { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("UDP senders and a receiver are created") {
      THEN ("the futures provide synchronization") {

        auto recv_endp = make_test_endpoint(test_port_base);
        auto recv_iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, recv_endp);

        REQUIRE_FALSE (recv_iohp->is_started());
        REQUIRE_FALSE (recv_iohp->is_io_started());

        auto recv_io_fut = chops::net::make_udp_io_interface_future(chops::net::udp_entity(recv_iohp));
        auto recv_io = recv_io_fut.get(); // UDP receiver is started

        REQUIRE (recv_iohp->is_started());

        msg_hdlr<chops::net::udp_io> recv_mh(reply);
        recv_iohp->start_io(MaxSize, std::ref(recv_mh));

        REQUIRE (recv_io.is_io_started());

        chops::net::send_to_all send_to_all_ios { };

        std::vector<chops::net::detail::udp_entity_io_ptr> senders;

// std::cerr << "creating " << num_senders << " senders" << std::endl;
        chops::repeat(num_senders, [&] (int i) {
            ip::udp::endpoint send_endp = reply ? 
                ip::udp::endpoint(ip::udp::v4(), static_cast<unsigned short>(test_port_base + i + 1)) :
                ip::udp::endpoint();

            auto send_iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, send_endp);
            REQUIRE_FALSE (send_iohp->is_started());
            REQUIRE_FALSE (send_iohp->is_io_started());

            senders.push_back(send_iohp);

            auto send_io_fut = chops::net::make_udp_io_interface_future(chops::net::udp_entity(send_iohp));
            auto send_io = send_fut.get();

            if (reply) {
              send_io.start_io(MaxSize, recv_endp, msg_hdlr<chops::net::udp_io>(false));
            }
            else {
              send_io.start_io(recv_endp);
            }

            REQUIRE (send_io.is_started());

            send_to_all_ios.add_io_interface(send_io);
        }
        // send messages through all of the senders
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        for (auto buf : in_msg_set) {
          send_to_all_ios.send(buf);
          std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        // poll output queue size of all handlers until 0
        while (send_to_all_ios.total_output_queue_size() > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // stop all handlers
        for (auto p : senders) {
          p->stop();
          REQUIRE_FALSE (p->is_started());
          REQUIRE_FALSE (p->is_io_started());
        }
        recv_iohp->stop();
        REQUIRE_FALSE (recv_iohp->is_started());
        REQUIRE_FALSE (recv_iohp->is_io_started());

// std::cerr << "Udp entities stopped, returning msgs size" << std::endl;

        std::size_t total_msgs = num_senders * in_msg_set.size();
        // CHECK instead of REQUIRE since UDP is an unreliable protocol
        CHECK (recv_mh.msgs.size() == total_msgs);
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

  auto ipv4_endp = make_test_endpoint(test_port_base);
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

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  udp_test ( ms, false, 50, 1);

}

SCENARIO ( "Udp IO handler test, var len msgs, one-way, interval 0, senders 1",
           "[udp_io] [var_len_msg] [one-way] [interval_0] [senders_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  udp_test ( ms, false, 0, 1);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 50, senders 10",
           "[udp_io] [var_len_msg] [two-way] [interval_50] [senders_10]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  udp_test ( ms, true, 50, 10);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 20, senders 5, many msgs",
           "[udp_io] [var_len_msg] [two_way] [interval_20] [senders_5] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  udp_test ( ms, true, 20, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, one-way, interval 100, senders 5",
           "[udp_io] [cr_lf_msg] [one-way] [interval_100] [senders_5]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  udp_test ( ms, false, 100, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 20, senders 5",
           "[udp_io] [cr_lf_msg] [two-way] [interval_20] [senders_5]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  udp_test ( ms, true, 20, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 0, senders 1, many msgs",
           "[udp_io] [cr_lf_msg] [two_way] [interval_0] [senders_1] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  udp_test ( ms, true, 0, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, one-way, interval 50, senders 1",
           "[udp_io] [lf_msg] [two-way] [interval_50] [senders_1]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  udp_test ( ms, false, 50, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 30, senders 10",
           "[udp_io] [lf_msg] [two-way] [interval_30] [senders_10]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  udp_test ( ms, true, 30, 10);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 200, senders 2, many msgs",
           "[udp_io] [lf_msg] [two-way] [interval_200] [senders_2] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 30*NumMsgs);
  udp_test ( ms, true, 200, 2);

}

