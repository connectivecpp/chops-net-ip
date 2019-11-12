/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c tcp_connector_timeout class.
 *
 *  @author Nathan Deutsch
 *
 *  Copyright (c) 2017-2019 by Cliff Green, Nathan Deutsch
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED
#define TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <cmath>
#include <chrono>
#include <optional>

namespace chops {
namespace net {

/*
 *  @brief Class for calculating TCP reconnection attempt timeout values.
 *
 *  When a TCP connect attempt fails within a TCP connector, either the connector should quit
 *  attempting to connect, or should wait for a period of time before attempting another connect.
 *  This class allows multiple behaviors depending on the constructor parameters.
 *
 *  If an application desires different behavior than what is provided by this class, the
 *  @c make_tcp_connector methods in the @c net_ip class take a function object allowing any application 
 *  defined logic.
 */
class tcp_connector_timeout {
public:

//   tcp_connector_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(500))

// , std::chrono::milliseconds max_timeout = std::chrono::milliseconds(60 * 1000), 
/*
 * More doxygen to be filled in here
 */
  tcp_connector_timeout(std::chrono::milliseconds initial_timeout = std::chrono::milliseconds(500), std::chrono::milliseconds max_timeout = std::chrono::milliseconds(60 * 1000), 
  bool reconn=true, bool backoff=true): m_initial_timeout(initial_timeout), m_max_timeout(max_timeout), m_reconn(reconn), m_backoff(backoff) {}

/*
 *  @brief Function call interface, used within the TCP connector functionality.
 *
 *  This method is not directly called by application code. It is called by the TCP connector
 *  internal functionality.
 *
 *  @param num_connects A count of how many times a reconnect has been attempted (the initial TCP connect is 
 *  immediately attempted, this method is called after an initial connect attempt fails).
 *
 *  @return The number of milliseconds the TCP connector should wait before attempting another connect. If the
 *  @c optional is empty, connect attempts are stopped.
 *
 */
  std::optional<std::chrono::milliseconds> operator()(std::size_t num_connects) const noexcept {
    if(!m_reconn && num_connects > 0) {
      return {};
    }

    if(!m_backoff || num_connects == 0) { 
      return std::optional<std::chrono::milliseconds> {m_initial_timeout};
    }

    return exponential_backoff(num_connects + 1);
  } //end operator()

private:
  const std::chrono::milliseconds m_initial_timeout;
  const std::chrono::milliseconds m_max_timeout;
  const bool m_reconn;
  const bool m_backoff;

  std::optional<std::chrono::milliseconds> exponential_backoff(std::size_t power) const noexcept { 
    int initial = static_cast<int>(m_initial_timeout.count());
    int max = static_cast<int>(m_max_timeout.count());

    int ms = std::pow(initial, power);
  
    if(ms > max) { 
      ms = max;
    }

    return std::optional<std::chrono::milliseconds>{ms};
  }

}; // end tcp_connector_timeout

} // end net namespace
} // end chops namespace

#endif

