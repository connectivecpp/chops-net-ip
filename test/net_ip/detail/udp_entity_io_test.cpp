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

#include "net_ip/worker.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"


#include <iostream>

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

using start_prom_t = std::promise<chops::net::udp_io_interface>;

struct state_chg_cb {
  bool              m_receive;
  ip::udp::endpoint m_dest_endp;
  start_prom_t      m_prom;
  msg_hdlr<chops::net::detail::udp_entity_io>
                    m_hdlr;


  state_chg_cb (bool reply, bool receive, ip::udp::endpoint dest_endp, 
                start_prom_t p = start_prom_t()) : 
      m_receive(receive), m_dest_endp(dest_endp), m_prom(std::move(p)), m_hdlr(reply) { }

  void operator()(chops::net::udp_io_interface io, std::size_t sz) {
    if (m_receive) {
      io.start_io(MaxSize, m_dest_endp, std::ref(m_hdlr));
    }
    else {
      io.start_io(m_dest_endp);
    }
std::cerr << "In start chg cb, sz = " << sz << std::endl;
    m_prom.set_value(io);
  }

  void operator()(chops::net::udp_io_interface io, std::error_code e, std::size_t sz) const {
std::cerr << "In shutdown chg cb, err = " << e << ", " << e.message() << ", sz = " << 
sz << std::endl;
  }

};

void null_start_cb(chops::net::udp_io_interface, std::size_t) { }
void null_shutdown_cb(chops::net::udp_io_interface, std::error_code, std::size_t) { }

std::size_t sender_func (const vec_buf& in_msg_set, io_context& ioc, 
                  ip::udp::endpoint dest_endp, ip::udp::endpoint sender_endp,
                  int interval) {

  bool receive = (sender_endp != ip::udp::endpoint());

  auto iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, sender_endp);

  start_prom_t start_prom;
  auto start_fut = start_prom.get_future();

  state_chg_cb cb(false, receive, dest_endp, std::move(start_prom));

  iohp->start(std::ref(cb), std::ref(cb));

  auto io = start_fut.get();

std::cerr << "Sender start chg future popped, io valid: " << std::boolalpha << 
io.is_valid() << std::endl;

  for (auto buf : in_msg_set) {
    io.send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  // poll output queue size until it is 0
  std::size_t qsz = 1;
  while (qsz > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto qstats = io.get_output_queue_stats();
    qsz = qstats.output_queue_size;
  }
  iohp->stop();

std::cerr << "Sender udp entity stopped, returning msgs size" << std::endl;

  return cb.m_hdlr.msgs.size();
}

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

        auto receiver_endp = make_test_endpoint(test_port_base);
        auto iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, receiver_endp);

        REQUIRE_FALSE (iohp->is_started());
        REQUIRE_FALSE (iohp->is_io_started());

        iohp->start(null_start_cb, null_shutdown_cb);
        REQUIRE (iohp->is_started());

        msg_hdlr<chops::net::detail::udp_entity_io> msg_hdlr(reply);
        iohp->start_io(MaxSize, std::ref(msg_hdlr));
        REQUIRE (iohp->is_io_started());


        std::vector<std::future<std::size_t> > sender_futs;

std::cerr << "creating " << num_senders << " async futures and threads" << std::endl;
        chops::repeat(num_senders, [&] (int i) {
            ip::udp::endpoint sender_endp = reply ? 
                ip::udp::endpoint(ip::udp::v4(), static_cast<unsigned short>(test_port_base + i + 1)) :
                ip::udp::endpoint();
            sender_futs.emplace_back(std::async(std::launch::async, sender_func, 
                                     std::cref(in_msg_set), std::ref(ioc), 
                                     receiver_endp, sender_endp,
                                     interval));
          }
        );

        std::size_t accum_msgs = 0;
        for (auto& fut : sender_futs) {
          accum_msgs += fut.get(); // wait for senders to finish
        }
        INFO ("All sender futures popped");
std::cerr << "All sender futures popped" << std::endl;

        iohp->stop();

        INFO ("Receiver stopped");
std::cerr << "Receiver stopped" << std::endl;

        REQUIRE_FALSE(iohp->is_started());
        REQUIRE_FALSE (iohp->is_io_started());

        std::size_t total_msgs = num_senders * in_msg_set.size();
        REQUIRE (msg_hdlr.msgs.size() == total_msgs);
        if (reply) {
          REQUIRE (accum_msgs == total_msgs);
        }
        else {
          REQUIRE (accum_msgs == 0);
        }
      }
    }
  } // end given

//  wg.reset();
//  thr.join();
  wk.reset();
//  wk.stop();

}


void udp_test (const vec_buf& in_msg_set, bool reply, int interval, int num_senders);

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

SCENARIO ( "Udp IO handler test, var len msgs, two-way, interval 0, senders 20, many msgs",
           "[udp_io] [var_len_msg] [two_way] [interval_0] [senders_20] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  udp_test ( ms, true, 0, 20);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, one-way, interval 50, senders 30",
           "[udp_io] [cr_lf_msg] [one-way] [interval_50] [senders_30]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  udp_test ( ms, false, 50, 30);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 0, senders 25",
           "[udp_io] [cr_lf_msg] [two-way] [interval_0] [senders_25]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  udp_test ( ms, true, 0, 25);

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, two-way, interval 0, senders 10, many msgs",
           "[udp_io] [cr_lf_msg] [two_way] [interval_0] [senders_10] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  udp_test ( ms, true, 0, 10);

}

SCENARIO ( "Udp IO handler test, LF msgs, one-way, interval 50, senders 1",
           "[udp_io] [lf_msg] [two-way] [interval_50] [senders_1]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  udp_test ( ms, false, 50, 1);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 0, senders 20",
           "[udp_io] [lf_msg] [two-way] [interval_0] [senders_20]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  udp_test ( ms, true, 0, 20);

}

SCENARIO ( "Udp IO handler test, LF msgs, two-way, interval 0, senders 40, many msgs",
           "[udp_io] [lf_msg] [two-way] [interval_0] [senders_40] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  udp_test ( ms, true, 0, 40);

}

