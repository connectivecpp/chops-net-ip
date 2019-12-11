/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Parse command line arguments for the data sender / receiver (DSR), both
 *  TCP and UDP (UDP to be implemented later).
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef DSR_ARGS_HPP_INCLUDED
#define DSR_ARGS_HPP_INCLUDED

#include <string>
#include <cstddef> // std::size_t
#include <vector>
#include <chrono>

namespace chops {
namespace test {

struct connector_info {
  std::string     m_port;
  std::string     m_host;
};

// command line argument data for use by TCP DSR
struct tcp_dsr_args {
  std::string     m_dsr_name;      // name of this DSR instance
  bool            m_reply;         // if true, incoming messages reflected back to sender
  int             m_modulus;       // how often to send log msg to monitor, both outgoing and incoming messages
  int             m_send_count;    // if 0, don't send any messages
  std::chrono::milliseconds
                  m_delay;
  std::vector<std::string>
                  m_acceptors;     // 0 - N TCP acceptors, port for each entry
  std::vector<connector_info>
                  m_connectors;    // 0 - N TCP connectors, port and host for each entry
  connector_info  m_monitor;       // monitor port, host
};


// besides filling in the command line parsing, decide on error handling - possibilities include
// return std::optional<tcp_dsr_args>, or return std::expected<tcp_dsr_args>, or throwing an 
// exception
inline tcp_dsr_args parse_command_line (int argc, char* argvp[]) {
  tcp_dsr_args args { };
  return args;
}

inline void print_help () {

}


} // end namespace test
} // end namespace chops

#endif

