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
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include "asio/ip/udp.hpp"
#include "asio/io_context.hpp"

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move
#include <thread>
#include <future> // std::async
#include <chrono>
#include <vector>
#include <functional> // std::ref, std::cref
#include <numeric> // std::accumulate

#include <cassert>

#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/io_type_decls.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/start_funcs.hpp"

#include "marshall/shared_buffer.hpp"

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

#include <iostream> // std::err for error sink

using namespace asio;
using namespace chops::test;

// using notifier_cb = 
//   typename chops::net::detail::io_base<chops::net::detail::udp_io>::entity_notifier_cb;

const char*   test_addr = "127.0.0.1";
constexpr int test_port_base = 30665;
constexpr int NumMsgs = 50;

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

using iosp = chops::net::detail::udp_entity_io_shared_ptr;

void start_udp_senders(const vec_buf& in_msg_vec, bool reply, int interval, int num_senders,
                       test_counter& send_cnt, io_context& ioc, 
                       chops::net::err_wait_q& err_wq, const ip::udp::endpoint& recv_endp) {

  std::vector<iosp> senders;

  chops::repeat(num_senders, [&ioc, &senders, &send_cnt, &err_wq] (int i) {
      std::string port_num = std::to_string(test_port_base + i + 1);
      auto send_ptr = std::make_shared<chops::net::detail::udp_entity_io>(ioc, port_num, test_addr);
      senders.push_back(send_ptr);
      send_ptr->start([&send_cnt] (chops::net::udp_io_interface io, std::size_t, bool starting) {
                if (starting) {
                  auto r = udp_start_io(io, false, send_cnt);
                  assert (r);
                }
                return true; }
            , chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq));
    }
  );
  // send messages through all of the senders
  std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  for (auto buf : in_msg_vec) {
    for (auto ioh : senders) {
      ioh->send(buf, recv_endp);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }

  // poll output queue size of all handlers until 0
  std::size_t sum = 0;
  while ((sum = std::accumulate(senders.begin(), senders.end(), 0u,
			  [] (std::size_t s, const iosp& p) { return s + (p->get_output_queue_stats()).output_queue_size; } )) > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cerr << "****** Senders total output queue size: " << sum << std::endl;
  }
  std::cerr << "****** Senders total output queue size is now 0" << std::endl;
  // stop all handlers
  for (auto p : senders) {
    p->stop();
  }
}

void udp_test (const vec_buf& in_msg_vec, bool reply, int interval, int num_senders) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("UDP senders and a receiver are created") {
      THEN ("the futures provide synchronization") {

        auto recv_endp = make_udp_endpoint(test_addr, test_port_base);
        auto recv_ptr = std::make_shared<chops::net::detail::udp_entity_io>(ioc, recv_endp);

        INFO ("Receiving UDP entity created");

        chops::net::err_wait_q err_wq;
        auto err_fut = std::async(std::launch::async,
          chops::net::ostream_error_sink_with_wait_queue,
          std::ref(err_wq), std::ref(std::cerr));

        test_counter recv_cnt = 0;
        recv_ptr->start(
              [&recv_cnt, reply] (chops::net::udp_io_interface io, std::size_t, bool starting) {
                if (starting) {
                  auto r = udp_start_io(io, reply, recv_cnt);
                  assert (r);
                }
                return true;
              },
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(err_wq)
        );

  // sleep to make sure recv UDP is up and running; yes, it could be done with futures, but keeping
  // it simple for this unit test
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        test_counter send_cnt = 0;

        INFO ("Starting first iteration of UDP senders, num: " << num_senders);
        start_udp_senders(in_msg_vec, reply, interval, num_senders,
                          send_cnt, ioc, err_wq, recv_endp);
        INFO ("Starting second iteration of UDP senders");
        start_udp_senders(in_msg_vec, reply, interval, num_senders,
                          send_cnt, ioc, err_wq, recv_endp);


        INFO ("Pausing, then stopping receiver");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        recv_ptr->stop();


        INFO ("Waiting on error wait queue");
        while (!err_wq.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        err_wq.close();
        auto cnt = err_fut.get();
        INFO ("Number of messages passed thru error queue: " << cnt);

        std::size_t total_msgs = 2 * num_senders * in_msg_vec.size();
        // CHECK instead of REQUIRE since UDP is an unreliable protocol
        CHECK (total_msgs == recv_cnt);
        if (reply) {
          CHECK (total_msgs == send_cnt);
        }
      }
    }
  } // end given

  wk.reset();

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

SCENARIO ( "Udp IO handler test, var len msgs, one-way, interval 30, senders 1",
           "[udp_io] [var_len_msg] [one_way] [interval_30] [senders_1]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs),
             false, 30, 1);

}

SCENARIO ( "Udp IO handler test, var len msgs, one-way, interval 0, senders 1",
           "[udp_io] [var_len_msg] [one-way] [interval_0] [senders_1]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs),
             false, 0, 1);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 20, senders 10",
           "[udp_io] [var_len_msg] [two-way] [interval_20] [senders_10]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Yowser!", 'X', NumMsgs),
             true, 20, 10);

}

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 30, senders 2, many msgs",
           "[udp_io] [var_len_msg] [two_way] [interval_30] [senders_2] [many]" ) {

  udp_test ( make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 10*NumMsgs),
             true, 30, 2);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, one-way, interval 10, senders 5",
           "[udp_io] [cr_lf_msg] [one-way] [interval_10] [senders_5]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs),
             false, 10, 5);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 20, senders 5",
           "[udp_io] [cr_lf_msg] [two-way] [interval_20] [senders_5]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs),
             true, 20, 5);

}

/*
SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 0, senders 1, many msgs",
           "[udp_io] [cr_lf_msg] [two_way] [interval_0] [senders_1] [many]" ) {

  udp_test ( make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs),
             true, 0, 1);

}
*/

SCENARIO ( "Udp IO handler test, LF msgs, one-way, interval 30, senders 1",
           "[udp_io] [lf_msg] [two-way] [interval_30] [senders_1]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Excited!", 'E', NumMsgs),
             false, 30, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 10, senders 10",
           "[udp_io] [lf_msg] [two-way] [interval_10] [senders_10]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs),
             true, 10, 10);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 10, senders 2, many msgs",
           "[udp_io] [lf_msg] [two-way] [interval_10] [senders_2] [many]" ) {

  udp_test ( make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 20*NumMsgs),
             true, 10, 2);

}


