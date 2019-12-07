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
#include <optional>
#include <type_traits> // std::is_same

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

#include "asio/ip/basic_endpoint.hpp"

namespace chops {
namespace test {

enum msg_direction { incoming, outgoing };

struct monitor_msg_data {
  std::string                  m_dsr_name;
  std::string                  m_protocol; // "tcp" or "udp"
  std::string                  m_remote_host;
  std::string                  m_remote_port;
  msg_direction                m_direction;
  std::size_t                  m_curr_msg_num;
  std::size_t                  m_curr_msg_size;
  std::string                  m_curr_prefix;
  char                         m_curr_body_char;
  std::optional<std::size_t>   m_total_msgs_to_send;  // only meaningful for outgoing data
  std::optional<std::size_t>   m_outgoing_queue_size; // only meaningful for outgoing data

  template <typename AsioIpProtocol>
  monitor_msg_data (const asio::ip::basic_endpoint<AsioIpProtocol>& remote_endpoint,
                    const std::string& dsr_name,
                    msg_direction direction,
                    std::size_t curr_msg_num,
                    std::size_t curr_msg_size,
                    const std::string& curr_prefix,
                    char curr_body_char,
                    std::size_t total_msgs_to_send,
                    std::size_t outgoing_queue_size) :
      m_dsr_name(dsr_name),
      m_protocol("tcp"),
      m_remote_host(),
      m_remote_port(),
      m_direction(direction),
      m_curr_msg_num(curr_msg_num),
      m_curr_msg_size(curr_msg_size),
      m_curr_prefix(curr_prefix),
      m_curr_body_char(curr_body_char),
      m_total_msgs_to_send(total_msgs_to_send),
      m_outgoing_queue_size(outgoing_queue_size)
  {
    m_remote_host = remote_endpoint.address().to_string();
    m_remote_port = std::to_string(remote_endpoint.port());
    if constexpr (std::is_same_v<asio::ip::udp, AsioIpProtocol>) {
      m_protocol = "udp";
    }
  }
                    
  // more constructors to be added
};

struct shutdown_msg {
// to be filled in
};


// shutdown message from monitor process will cause promise to be set, which is signal to DSR to
// shutdown
class monitor_connector {
public:

  monitor_connector(chops::net::net_ip& net_ip, std::string_view monitor_port, std::string_view monitor_host,
                    std::promise<void> prom) : // fill in initializer list, promise must use std::move

  {
  }
  void send_monitor_msg (const monitor_msg_data& msg_data) const {

  }

private:
  chops::net::net_entity     m_monitor;
  chops::net::tcp_io_output  m_io_output;
  // more member data to fill in

};

inline chops::const_shared_buffer marshall_monitor_msg_data (const monitor_msg_data& msg_data) {

}

inline monitor_msg_data unmarshall_monitor_msg_data (const chops::const_shared_buffer& buf) {

}

} // end namespace test
} // end namespace chops

#endif

