/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Data structures sent between nodes in test data blaster
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019-2025 by Cliff Green, (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MONITOR_MSG_HPP_INCLUDED
#define MONITOR_MSG_HPP_INCLUDED

#include <string>
#include <string_view>

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


/*
inline chops::const_shared_buffer serialize_monitor_msg_data (const monitor_msg_data& msg_data) {
    // work in progress
    return null;
}

inline monitor_msg_data deserialize_monitor_msg_data (const chops::const_shared_buffer& buf) {
    // work in progress
    return null;
}

inline chops::const_shared_buffer serialize_shutdown_message() {
    // work in progress
    return null;
}

inline std::string deserialize_shutdown_message() {
  // work in progress
  return null;
}
*/


}
}
#endif

