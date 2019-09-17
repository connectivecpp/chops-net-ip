/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c default_connector_timeout class
 *
 *  @author Nathan Deutsch
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 #ifndef DEFAULT_CONNECTOR_TIMEOUT_HPP_INCLUDED
 #define DEFAULT_CONNECTOR_TIMEOUT_HPP_INCLUDED

 #include "asio/ip/basic_resolver.hpp"
 #include "asio/io_context.hpp"

 #include <string_view>
 #include <string>
 #include <utility>
 #include <math.h>       /* pow */

namespace chops {
namespace net {

  /*
   *  @brief Class that handles TCP reconnection attempts
   */
class tcp_connector_timeout {
public:
  tcp_connector_timeout((std::chrono::milliseconds initial_timeout, int max_timeout = (60 * 1000), 
  int multiplier = 2, bool reconn=false, bool backoff=false);

  std::optional<std::chrono::milliseconds> operator()(std::size_t num_connects) {
    //no reconnection attempt, exit gracefully
    if(!reconn) {
      return {};
    }

    if(!backoff) { 
      return std::optional<std::chrono::milliseconds> {initial_timeout};
    }

    //exponential backoff
    int ms = pow(multiplier, num_connects);
    
    if(ms > max_timeout) {
      ms = max_timeout;
    }
    
    return std::optional<std::chrono::milliseconds> {ms};
  } //end operator()

}; // end default_connector_timeout

} // end net namespace
} // end chops namespace

#endif
