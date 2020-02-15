/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Monitor message marshalling, unmarshalling, sending, and shutdown msg handling.
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by Cliff Green, (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MONITOR_MSG_HANDLING_HPP_INCLUDED
#define MONITOR_MSG_HANDLING_HPP_INCLUDED

#include <string>
#include <string_view>
#include <cstddef> // std::size_t
#include <future>
#include <utility> // std::move
#include <type_traits> // std::is_same

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "asio/ip/basic_endpoint.hpp"

namespace chops {
namespace test {

template <typename AsioIpProtocol>
std::string format_addr (const asio::ip::basic_endpoint<AsioIpProtocol>& endpoint) {
  return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

struct monitor_msg_data {

  static constexpr std::size_t max_msg_data_to_capture = 15;
  enum msg_direction { incoming, outgoing };

  std::string    m_dsr_name;
  std::string    m_protocol; // "tcp" or "udp"
  std::string    m_remote_addr; // in "host:port" format, see format_addr above
  msg_direction  m_direction;
  std::size_t    m_curr_msg_num;
  std::size_t    m_curr_msg_size;
  std::string    m_curr_msg_beginning; // up to max_msg_data_to_capture chars
  std::size_t    m_total_msgs_to_send;  // 0 if direction is incoming, since total not known in advance
  std::size_t    m_outgoing_queue_size;

};

struct shutdown_msg {
// to be filled in
};


// shutdown message from monitor process will set promise, which causes future to pop, which
// tells DSR to shutdown
class monitor_connector {
public:

  monitor_connector(chops::net::net_ip& net_ip,
                    std::string_view monitor_port, std::string_view monitor_host,
                    std::promise<void> prom,
                    chops::net::err_wait_q& err_wq) : 
      m_monitor(), m_io_output(), m_prom(std::move(prom))
  {
     // make_tcp_connector, start, provide state chg func obj that does start_io,
     chops::net::net_entity net_entity;
     net_entity = net_ip.make_tcp_connector(monitor_port, monitor_host);
     net_ip.start_io(HDR_SIZE, msg_hndlr, msg_frame);

     // providing msg handler for shutdown, make_err_func_with_wait_queue for error display
     chops::net::err_wait_q wq; // copied this from error_delivery_test
     auto err_func = chops::net::make_error_func_with_wait_queue<io_handler_mock>(wq);

     auto shutdown_fut = shutdown_prom.get_future();
  }
  void send_monitor_msg (const monitor_msg_data& msg_data) const {

  }
  // temporary - used for testing DSR
  void shutdown() {
  // send shutdown msg
    m_prom.set_value();
  }

private:
  chops::net::net_entity     m_monitor;
  chops::net::tcp_io_output  m_io_output;
  std::promise<void>         m_prom;
  // more member data to fill in

};

inline chops::const_shared_buffer marshall_monitor_msg_data (const monitor_msg_data& msg_data) {
// marshall transform to steam of bytes
}

inline monitor_msg_data unmarshall_monitor_msg_data (const chops::const_shared_buffer& buf) {
// transform bytes to obj
}

} // end namespace test
} // end namespace chops

#endif

