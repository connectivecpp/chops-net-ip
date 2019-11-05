/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c default_connector_timeout class
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

 #include <string_view>
 #include <string>
 #include <utility>
 #include <cmath>

namespace chops {
namespace net {

  /*
   *  @brief Class that handles TCP reconnection attempts
   */
class tcp_connector_timeout {
public:
  tcp_connector_timeout(std::chrono::milliseconds initial_timeout = 500, std::chrono::milliseconds max_timeout = (60 * 1000), 
  bool reconn=true, bool backoff=true);

  std::optional<std::chrono::milliseconds> operator()(std::size_t num_connects) {
    if(!reconn && num_connects > 0) {
      return {};
    }

    if(!backoff || num_connects == 0) { 
      return std::optional<std::chrono::milliseconds> {initial_timeout};
    }

    return exponential_backoff(num_connects + 1);
  } //end operator()

private:
  std::optional<std::chrono::milliseconds> exponential_backoff(int power) { 
    int ms = std::pow( (int)initial_timeout, power);

    if(ms > max_timeout) { 
      ms = max_timeout;
    }

    return std::optional<std::chrono::milliseconds>{ms};
  }

}; // end default_connector_timeout

} // end net namespace
} // end chops namespace

#endif
