/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test application that sends and receives data as well as sends progress messages to
 *  a monitor application.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <cstddef> // std::size_t
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <algorithm> // std::min

#include <iostream> // std::cerr for error sink

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/output_queue_stats.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "shared_test/msg_handling.hpp"
#include "marshall/shared_buffer.hpp"

#include "test_data_blaster/tcp_dsr_args.hpp"
#include "test_data_blaster/monitor_msg_handling.hpp"

const std::string_view msg_prefix { "Tasty testing!" };

void send_mon_msg (chops::test::monitor_connector& mon,
                   chops::test::monitor_msg_data& mon_msg,
                   int curr_msg_num, int mod,
                   chops::net::tcp_io_output io_out,
                   const std::byte* msg_ptr,
                   std::size_t msg_size) {

  // the +2 and -2 on msg pointer and size is adjusting for 16 bit binary length header
  if (curr_msg_num % mod == 0) {
    mon_msg.m_curr_msg_num = static_cast<std::size_t>(curr_msg_num);
    mon_msg.m_curr_msg_size = msg_size - 2u;
    mon_msg.m_curr_msg_beginning = std::string(chops::cast_ptr_to<char>(msg_ptr + 2),
                   std::min(chops::test::monitor_msg_data::max_msg_data_to_capture, (msg_size - 2u)));
    auto oqs = io_out.get_output_queue_stats();
    mon_msg.m_outgoing_queue_size = oqs ? (*oqs).output_queue_size : 0u;
    mon.send_monitor_msg(mon_msg);
  }
}

int send_msgs_func (chops::net::tcp_io_output io_out, char body_char,
                    int send_count, int mod,
                    std::chrono::milliseconds delay,
                    chops::test::monitor_connector& mon,
                    chops::test::monitor_msg_data mon_msg) {

  mon_msg.m_total_msgs_to_send = static_cast<std::size_t>(send_count);
  mon_msg.m_direction = chops::test::monitor_msg_data::outgoing;

  auto msgs = chops::test::make_msg_vec (chops::test::make_variable_len_msg, msg_prefix, 
                                         body_char, send_count);
  int num_sent { 0 };
  for (const auto& m : msgs) {
    if (!io_out.is_valid()) {
      break;
    }
    io_out.send(m);
    send_mon_msg(mon, mon_msg, ++num_sent, mod, io_out, m.data(), m.size());
    std::this_thread::sleep_for(delay);
  }
  return num_sent;
}

struct msg_hdlr {
  int                              m_count;
  bool                             m_reply;
  int                              m_modulo;
  chops::test::monitor_connector&  m_mon;
  chops::test::monitor_msg_data    m_mon_msg;

  msg_hdlr(bool reply, int mod, chops::test::monitor_connector& mon,
           chops::test::monitor_msg_data mon_msg) : 
        m_count(0), m_reply(reply), m_modulo(mod), m_mon(mon), m_mon_msg(mon_msg)
  {
    m_mon_msg.m_total_msgs_to_send = 0u;
    m_mon_msg.m_direction = chops::test::monitor_msg_data::incoming;
  }

  bool operator()(asio::const_buffer buf, chops::net::tcp_io_output io_out, const asio::ip::tcp::endpoint&) {
    
    if (m_reply) {
      io_out.send(buf.data(), buf.size());
    }
    send_mon_msg(m_mon, m_mon_msg, ++m_count, m_modulo, io_out,
                 chops::cast_ptr_to<std::byte>(buf.data()), buf.size());
    return true;
  }
};

using send_fut_vec = std::vector<std::future<int>>;

struct state_chg {
  int                              m_send_count;
  char                             m_body_char;
  std::chrono::milliseconds        m_delay;
  bool                             m_reply;
  int                              m_modulo;
  send_fut_vec&                    m_send_futs;
  chops::test::monitor_connector&  m_mon;
  chops::test::monitor_msg_data    m_mon_msg;

  state_chg(int send_count, char body_char, std::chrono::milliseconds delay,
            bool reply, int mod, send_fut_vec& send_futs,
            chops::test::monitor_connector& mon,
            chops::test::monitor_msg_data mon_msg) :
    m_send_count(send_count), m_body_char(body_char), m_delay(delay),
    m_reply(reply), m_modulo(mod), m_send_futs(send_futs),
    m_mon(mon), m_mon_msg(mon_msg) { }

  void operator() (chops::net::tcp_io_interface io_intf, std::size_t, bool starting) {
    if (starting) {
      std::string addr;
      io_intf.visit_socket([&addr] (asio::ip::tcp::socket& sock) {
            addr = chops::test::format_addr(sock.remote_endpoint());
        }
      );
      m_mon_msg.m_remote_addr = addr;
      io_intf.start_io(2u, msg_hdlr(m_reply, m_modulo, m_mon, m_mon_msg), 
                           chops::test::decode_variable_len_msg_hdr);
      if (m_send_count > 0) {
        auto send_fut = std::async(std::launch::async, send_msgs_func,
                                   *(io_intf.make_io_output()),
                                   m_body_char, m_send_count, m_modulo, m_delay,
                                   std::ref(m_mon), m_mon_msg);
        m_send_futs.push_back(std::move(send_fut));
      }
    }
  }

};

void start_entity (chops::net::net_entity ent, char body_char,
                   const chops::test::tcp_dsr_args& parms, send_fut_vec& send_futs,
                   chops::test::monitor_connector& mon, chops::test::monitor_msg_data mon_msg,
                   chops::net::err_wait_q& err_wq) {
  ent.start(state_chg(parms.m_send_count, body_char, parms.m_delay, parms.m_reply,
                      parms.m_modulus, send_futs, mon, mon_msg),
            chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(err_wq));
}

int main (int argc, char* argv[]) {

  auto parms = chops::test::parse_command_line(argc, argv);

  char body_char { 'a' };

  chops::net::worker wk;
  wk.start();
  chops::net::net_ip nip(wk.get_io_context());

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, chops::net::ostream_error_sink_with_wait_queue,
                            std::ref(err_wq), std::ref(std::cerr));

  std::promise<void> shutdown_prom;
  auto shutdown_fut = shutdown_prom.get_future();

  chops::test::monitor_connector mon(nip, parms.m_monitor.m_port, parms.m_monitor.m_host, 
                                     std::move(shutdown_prom), err_wq);

  chops::test::monitor_msg_data mon_msg;
  mon_msg.m_dsr_name = parms.m_dsr_name;
  mon_msg.m_protocol = "tcp";

  send_fut_vec send_futs;
  for (const auto& s : parms.m_acceptors) {
    auto ent = nip.make_tcp_acceptor(s);
    start_entity(ent, body_char++, parms, send_futs, mon, mon_msg, err_wq);
  }
  for (const auto& t : parms.m_connectors) {
    auto ent = nip.make_tcp_connector(t.m_port, t.m_host);
    start_entity(ent, body_char++, parms, send_futs, mon, mon_msg, err_wq);
  }

  // everything up and running, block waiting on shutdown msg
  shutdown_fut.get(); // monitor sent shutdown msg
  nip.stop_all(); // stop all entities

  int t_num { 0 };
  for (auto& t : send_futs) {
    auto num = t.get();
    std::cerr << "TCP DSR " << parms.m_dsr_name << 
      ", sending thread num " << ++t_num << " finished, num msgs sent: " << num << std::endl;
  }
  
  err_wq.close();
  auto err_cnt = err_fut.get();

  wk.reset();

  std::cerr << "TCP DSR " << parms.m_dsr_name << 
    ", shutting down, num error logs displayed: " << err_cnt << std::endl;

}

