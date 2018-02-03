/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_ip class.
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
#include <optional>


#include "net_ip/worker.hpp"
#include "net_ip/net_ip.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip/net_entity.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"


// #include <iostream>

using namespace std::experimental::net;
using namespace chops::test;

const char* tcp_test_port = "30565";
const char* tcp_test_host = "";
constexpr int NumMsgs = 50;
constexpr int ReconnTime = 500;

const char*   udp_test_addr = "ldskfjlk";
constexpr int udp_port_base = 31445;
constexpr int udp_max_size = 65507;

// Catch test framework not thread-safe, all REQUIRE clauses must be in single thread

void null_tcp_stop_func (chops::net::tcp_io_interface, std::error_code, std::size_t) { }
void null_udp_stop_func (chops::net::udp_io_interface, std::error_code, std::size_t) { }

ip::udp::endpoint make_test_udp_endpoint(int udp_port_num) {
  return ip::udp::endpoint(ip::make_address(udp_test_addr),
                           static_cast<unsigned short>(udp_port_num));
}

void start_io (chops::net::tcp_io_interface io, bool reply, std::string_view delim) {
  if (delim.empty()) {
    io.start_io(2, msg_hdlr<chops::net::detail::tcp_io>(reply), 
                chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr));
  }
  else {
    io.start_io(delim, msg_hdlr<chops::net::detail::tcp_io>(reply));
  }
}

template <typename IOH>
std::size_t send_func (const vec_buf& in_msg_set, chops::net::io_interface<IOH> io,
                       int interval, chops::const_shared_buffer empty_msg) {

  std::size_t cnt = 0;
  for (auto buf : in_msg_set) {
    io.send(std::move(buf));
    ++cnt;
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  io.send(empty_msg);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  return cnt;
}

using udp_io_promise = std::promise<chops::net::udp_io_interface>;

struct udp_start_cb {

  ip::udp::endpoint  m_dest_endp;
  std::size_t        m_sz;
  udp_io_promise     m_io_prom;

  udp_start_cb (ip::udp::endpoint dest_endp, udp_io_promise iop) : 
      m_dest_endp(dest_endp), m_sz(0), m_io_prom(std::move(iop))  { }

  udp_start_cb (std::size_t max_sz, udp_io_promise iop) : 
      m_dest_endp(), m_sz(max_sz), m_io_prom(std::move(iop))  { }

  void operator()(chops::net::udp_io_interface io, std::size_t sz) {
// std::cerr << "In udp start chg cb, sz = " << sz << std::endl;
    if (m_sz != 0) {
      io.start_io(m_sz, msg_hdlr<chops::net::detail::udp_entity_io>(false));
    }
    else {
      io.start_io(m_dest_endp);
    }
    m_io_prom.set_value(io);
  }

};

using tcp_io_promise = std::promise<chops::net::tcp_io_interface>;

struct tcp_start_cb {
  bool                           m_reply;
  std::string_view               m_delim;
  std::optional<tcp_io_promise>  m_io_prom;

  tcp_start_cb (bool reply, std::string_view delim, tcp_io_promise iop) : 
      m_reply(reply), m_delim(delim), m_io_prom(tcp_io_promise(std::move(iop)))  { }

  tcp_start_cb (bool reply, std::string_view delim) : 
      m_reply(reply), m_delim(delim), m_io_prom()  { }

  void operator() (chops::net::tcp_io_interface io, std::size_t n) {
// std::cerr << "Start_cb invoked, num hdlrs: " << n 
// << std::boolalpha << ", io is_valid: " << io.is_valid() << std::endl;
    start_io(io, m_reply, m_delim);
    if (m_io_prom) {
      m_io_prom->set_value(io);
    }
  }
};

void acc_conn_test (const vec_buf& in_msg_set, bool reply, int interval, int num_conns,
                    std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and one or more connectors are created") {
      THEN ("the futures provide synchronization and data returns") {

        chops::net::net_ip nip(ioc);

        auto acc = nip.make_tcp_acceptor(tcp_test_port, tcp_test_host);
        REQUIRE_FALSE(acc.is_started());

        acc.start( tcp_start_cb(reply, delim), null_tcp_stop_func );
// std::cerr << "acceptor created" << std::endl;

        REQUIRE(acc.is_started());

        std::vector< chops::net::tcp_connector_net_entity > conns;
        std::vector< std::future<std::size_t> > futs;

// std::cerr << "creating " << num_conns << " connectors" << std::endl;

        chops::repeat(num_conns, [&] () {
            auto conn = nip.make_tcp_connector(tcp_test_port, tcp_test_host,
                                               std::chrono::milliseconds(ReconnTime));
            conns.push_back(conn);
            tcp_io_promise prom;
            auto io_fut = prom.get_future();
            conn.start( tcp_start_cb(false, delim, std::move(prom)), null_tcp_stop_func );
            auto io = io_fut.get();
// std::cerr << "Connector start promise popped, passing io_interface to sender func" << std::endl;
            futs.emplace_back(std::async(std::launch::async, send_func<chops::net::detail::tcp_io>, 
                              std::cref(in_msg_set), io, interval, empty_msg));
// std::cerr << "Sender thread started" << std::endl;
          
          }
        );

        std::size_t accum_msgs = 0;
        for (auto& fut : futs) {
          accum_msgs += fut.get(); // wait for senders to finish
        }
        INFO ("All sender futures popped");
// std::cerr << "All sender futures popped" << std::endl;

        for (auto conn : conns) {
          conn.stop();
          REQUIRE_FALSE(conn.is_started());
        }
// std::cerr << "All connectors stopped" << std::endl;

        acc.stop();
// std::cerr << "Acceptor stopped" << std::endl;
        REQUIRE_FALSE(acc.is_started());
        INFO ("Acceptor stopped");

        nip.remove(acc);
        nip.remove_all();
    
        std::size_t total_msgs = num_conns * in_msg_set.size();
        REQUIRE (accum_msgs == total_msgs);
      }
    }
  } // end given
  wk.stop();
}

void udp_test (const vec_buf& in_msg_set, int interval, int num_udp_pairs, 
               chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("pairs of senders and receivers are created") {
      THEN ("the futures provide synchronization and data returns") {

        chops::net::net_ip nip(ioc);

        std::vector< std::future<std::size_t> > futs;

// std::cerr << "creating " << num << " udp sender receiver pairs" << std::endl;

        chops::repeat(num_udp_pairs, [&] () {
            auto recv_endp = make_test_udp_endpoint(udp_port_base);

            auto udp_receiver = nip.make_udp_unicast(recv_endp);
            auto udp_sender = nip.make_udp_sender();

            udp_io_promise recv_prom;
            auto recv_fut = recv_prom.get_future();
            udp_receiver.start( udp_start_cb (udp_max_size, std::move(recv_prom)), null_udp_stop_func );

            recv_fut.get();
            // nothing needed at this point for receiver, msg handler is ready to go

            udp_io_promise send_prom;
            auto send_fut = send_prom.get_future();
            udp_sender.start( udp_start_cb (recv_endp, std::move(send_prom)), null_udp_stop_func );

            auto io = send_fut.get();

            futs.emplace_back(std::async(std::launch::async, send_func<chops::net::detail::udp_entity_io>, 
                              std::cref(in_msg_set), io, interval, empty_msg));
// std::cerr << "Sender thread started" << std::endl;
          
          }
        );

        std::size_t accum_msgs = 0;
        for (auto& fut : futs) {
          accum_msgs += fut.get(); // wait for senders to finish
        }
        INFO ("All sender futures popped");
// std::cerr << "All sender futures popped" << std::endl;

        nip.remove_all();
    
        std::size_t total_msgs = num_udp_pairs * in_msg_set.size();
        REQUIRE (accum_msgs == total_msgs);
      }
    }
  } // end given
  wk.stop();
}


SCENARIO ( "Net IP acceptor connector test, var len msgs, one-way, interval 50, 1 connector", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, var len msgs, one-way, interval 0, 1 connector", 
           "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Haw!", 'R', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 0, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, var len msgs, two-way, interval 50, 1 connector", 
           "[netip_acc_conn] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Yowser!", 'X', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 50, 1, std::string_view(), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, var len msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[netip_acc_conn] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Whoah, fast!", 'X', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, 10, std::string_view(), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, var len msgs, two-way, interval 0, 40 connectors", 
           "[netip_acc_conn] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Many, many, fast!", 'G', 100*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, true, 0, 40, std::string_view(), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, CR / LF msgs, one-way, interval 50, 1 connectors", 
           "[netip_acc_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, CR / LF msgs, one-way, interval 50, 10 connectors", 
           "[netip_acc_conn] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Hohoho!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 50, 10, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, CR / LF msgs, one-way, interval 0, 20 connectors", 
           "[netip_acc_conn] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, false, 0, 20, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, CR / LF msgs, two-way, interval 30, 10 connectors", 
           "[netip_acc_conn] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yowzah!", 'G', 5*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 30, 10, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test, CR / LF msgs, two-way, interval 0, 10 connectors, many msgs", 
           "[netip_acc_conn] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  auto ms = make_msg_set (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 200*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_cr_lf_text_msg));
  acc_conn_test ( ms, true, 0, 10, std::string_view("\r\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test,  LF msgs, one-way, interval 50, 1 connectors", 
           "[netip_acc_conn] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited!", 'E', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 50, 1, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test,  LF msgs, one-way, interval 0, 25 connectors", 
           "[netip_acc_conn] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Excited fast!", 'F', 6*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, false, 0, 25, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test,  LF msgs, two-way, interval 20, 15 connectors", 
           "[netip_acc_conn] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Whup whup!", 'T', 2*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 20, 15, std::string_view("\n"), empty_msg );

}

SCENARIO ( "Net IP acceptor connector test,  LF msgs, two-way, interval 0, 15 connectors, many msgs", 
           "[netip_acc_conn] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  auto ms = make_msg_set (make_lf_text_msg, "Super fast!", 'S', 300*NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_lf_text_msg));
  acc_conn_test ( ms, true, 0, 15, std::string_view("\n"), empty_msg );

}
