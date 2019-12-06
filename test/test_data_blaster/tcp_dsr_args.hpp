/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Parse command line arguments for the TCP data sender / receiver (DSR).
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_DSR_ARGS_HPP_INCLUDED
#define TCP_DSR_ARGS_HPP_INCLUDED

#include <string>
#include <cstddef> // std::size_t
#include <vector>
#include <optional>

namespace chops {
namespace test {

constexpr int prefix_max_size {20}; // arbitrary max size - we can adjust as needed

struct sending_parms {
  int           m_send_count;    // num msgs to send
  std::string   m_prefix;        // prefix string, same for each message
  char          m_body_char;     // each message will be filled with this char, after the prefix
  int           m_delay;         // delay in milliseconds between messages
};

struct connector_info {
  std::string     m_port;
  std::string     m_host;
};

// command line argument data for use by TCP DSR
struct tcp_dsr_args {
  std::string     m_dsr_name;      // name of this DSR instance
  bool            m_reply;         // if true, incoming messages are reflected back to sender
  int             m_modulus;       // how often to send log msg to monitor, both outgoing and incoming messages
  std::optional<sending_parms>
                  m_sending_parms; // if present - count, prefix, body char
  std::vector<std::string>
                  m_acceptors;     // 0 - N TCP acceptors, port for each entry
  std::vector<connector_info>
                  m_connectors;    // 0 - N TCP connectors, port and host for each entry
  connector_info  m_monitor;       // monitor port, host
};


// besides filling in the command line parsing, decide on error handling - possibilities include
// return std::optiona<tcp_dsr_args>, or throwing an exception
inline tcp_dsr_args parse_command_line (int argc, char* argvp[]) {

}

inline void print_help () {

}


} // end namespace test
} // end namespace chops

#endif

