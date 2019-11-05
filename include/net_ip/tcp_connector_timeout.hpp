/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c tcp_connector_timeout class
 *
 *  @author Nathan Deutsch
 *
 *  Copyright (c) 2017-2019 by Cliff Green and Nathan Deutsch
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 #ifndef TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED
 #define TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED

 #include <cmath>
 #include <chrono>
 #include <optional>

namespace chops {
namespace net {

  /*
   *  @brief Class that handles TCP reconnection attempts
   */
class tcp_connector_timeout {
public:
  tcp_connector_timeout(std::chrono::milliseconds initial_timeout = std::chrono::milliseconds(500), std::chrono::milliseconds max_timeout = std::chrono::milliseconds(60 * 1000), 
  bool reconn=true, bool backoff=true): m_initial_timeout(initial_timeout), m_max_timeout(max_timeout), m_reconn(reconn), m_backoff(backoff) {}

  std::optional<std::chrono::milliseconds> operator()(std::size_t num_connects) {
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

  std::optional<std::chrono::milliseconds> exponential_backoff(std::size_t power) { 
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
