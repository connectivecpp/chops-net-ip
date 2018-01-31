/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c udp_entity_io detail class.
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
  return ip::udp::endpoint(ip::address:make_address(test_addr),
                           static_cast<unsigned short>(port_num));
}

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread;

using start_prom_t = std::promise<chops::net::udp_io_interface>;

struct start_chg_cb {

  bool              m_reply;
  bool              m_receive;
  ip::udp::endpoint m_dest_endp;
  start_prom_t      m_prom;


  start_chg_cb (bool reply, bool receive, ip::udp::endpoint dest_endp, 
                start_prom_t p = start_prom_t()) : 
      m_reply(reply), m_receive(receive), m_dest_endp(dest_endp), m_prom(std::move(p)) { }

  void operator()(chops::net::udp_io_interface io, std::size_t sz) {
    if (m_receive) {
      io.start_io(MaxSize, m_dest_endp, msg_hdlr<chops::net::detail::udp_io>(reply));
    }
    else {
      io.start_io(m_dest_endp);
    }
std::cerr << "In start chg cb, sz = " << sz << std::endl;
    m_prom.set_value(io);
  }

};

struct shutdown_chg_cb {

  prom_type m_prom;

  shutdown_chg_cb (prom_type p = prom_type()) : m_prom(std::move(p)) { }

  void operator()(chops::net::udp_io_interface io, std::error_code e, std::size_t sz) {
std::cerr << "In shutdown chg cb, err = " << e << ", " << e.message() << ", sz = " << 
sz << std::endl;
    m_prom.set_value(io);
  }

};

bool sender_func (const vec_buf& in_msg_set, io_context& ioc, 
                  ip::udp::endpoint dest_endp, ip::udp::endpoint sender_endp,
                  int interval, chops::const_shared_buffer empty_msg) {

  bool receive = (sender_endp != ip::udp::endpoint());

  auto iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, sender_endp);

  prom_type start_prom;
  auto start_fut = start_prom.get_future();
  prom_type shutdown_prom;
  auto shutdown_fut = shutdown_prom.get_future();

  iohp->start(start_chg_cb(false, receive, dest_endp, std::move(start_prom)),
              shutdown_chg_cb(std::move(shutdown_prom)));

  auto io = start_fut.get();
std::cerr << "Start chg future popped" << std::endl;
  for (auto buf : in_msg_set) {
    io.send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  io.send(empty_msg);

  if (receive) {
    io = shutdown_fut.get();
std::cerr << "Shutdown chg future popped" << std::endl;
    iohp->stop();
  else {
    iohp->stop();
    io = shutdown_fut.get();
std::cerr << "Shutdown chg future popped" << std::endl;
  }
  return !io.is_valid();
}

void udp_test (const vec_buf& in_msg_set, bool reply, int interval, 
               int num_senders, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();
//  io_context ioc;
//  auto wg = make_work_guard(ioc);
//  std::thread thr( [&ioc] { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("a UDP sender and receiver are created") {
      THEN ("the futures provide synchronization") {

        auto receiver_endp = make_test_endpoint(test_port_base);
        auto iohp = std::make_shared<chops::net::detail::udp_entity_io>(ioc, receiver_endp);

        REQUIRE_FALSE (iohp->is_started());
        REQUIRE_FALSE (iohp->is_io_started());

        iohp->start(start_chg_cb(reply, true, ip::udp::endpoint()),
                    shutdown_chg_cb());

        REQUIRE (iohp->is_started());

        std::vector<std::future<bool> > sender_futs;

std::cerr << "creating " << num_senders << " async futures and threads" << std::endl;
        chops::repeat(num_senders, [&] (int i) {
            ip::udp::endpoint sender_endp = reply ? 
                ip::udp::endpoint(ip::udp::v4(), static_cast<unsigned short>(test_port_base + i + 1)) :
                ip::udp::endpoint();
            sender_futs.emplace_back(std::async(std::launch::async, sender_func, 
                                     std::cref(in_msg_set), std::ref(ioc), 
                                     receiver_endp, sender_endp,
                                     interval, empty_msg));
          }
        );

        for (auto& fut : conn_futs) {
          REQUIRE (fut.get()); // wait for senders to finish
        }
        INFO ("All sender futures popped");
std::cerr << "All sender futures popped" << std::endl;

        iohp->stop();

        INFO ("Receiver stopped");
std::cerr << "Receiver stopped" << std::endl;

        REQUIRE_FALSE(iohp->is_started());
      }
    }
  } // end given

//  wg.reset();
//  thr.join();
  wk.reset();
//  wk.stop();

}


SCENARIO ( "Udp IO handler test, variable len msgs, interval 50, one-way",
           "[udp_io] [one_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 0, one-way",
           "[udp_io] [one_way] [var_len_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 50, two-way",
           "[udp_io] [two_way] [var_len_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 50, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, variable len msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [var_len_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, std::string_view(), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 50, one-way",
           "[udp_io] [one_way] [cr_lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 0, one-way",
           "[udp_io] [one_way] [cr_lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 30, two-way",
           "[udp_io] [two_way] [cr_lf_msg] [interval_30]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 30, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, CR / LF msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [cr_lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 50, one-way",
           "[udp_io] [one_way] [lf_msg] [interval_50]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 50, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 0, one-way",
           "[udp_io] [one_way] [lf_msg] [interval_0]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 0, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 20, two-way",
           "[udp_io] [two_way] [lf_msg] [interval_20]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 20, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Udp IO handler test, LF msgs, interval 0, two-way, many msgs",
           "[udp_io] [two_way] [lf_msg] [interval_0] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 0, std::string_view("\n"), empty_msg );

}

