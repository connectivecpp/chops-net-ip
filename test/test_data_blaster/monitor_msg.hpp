/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Monitor message marshalling, unmarshalling, and sending.
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by Cliff Green, (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <string>
#include <string_view>
#include <cstddef> // std::size_t

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

namespace chops {
namespace test {

enum protocol_type { tcp, udp };
enum msg_direction { incoming, outgoing };

struct monitor_msg_data {
  std::string     m_dsr_name;
  protocol_type   m_protocol;
  std::string     m_remote_host;
  std::string     m_remote_port;
  msg_direction   m_direction;
  std::size_t     m_curr_msg_num;
  std::size_t     m_total_msgs;
  std::size_t     m_curr_msg_size;
  std::string     m_curr_prefix;
  char            m_curr_body_char;
};

class monitor_sender {
public:

  monitor_sender(chops::net::net_ip& net_ip, std::string_view monitor_port, std::string_view monitor_host) :

  {
  }
  void send_monitor_msg (const monitor_msg_data& msg_data) const {

  }

private:
  chops::net::net_entity     m_monitor;
  chops::net::tcp_io_output  m_io_output;

};

inline chops::const_shared_buffer marshall_monitor_msg_data (const monitor_msg_data& msg_data) {

}

inline monitor_msg_data unmarshall_monitor_msg_data (const chops::const_shared_buffer& buf) {

}

} // end namespace test
} // end namespace chops

