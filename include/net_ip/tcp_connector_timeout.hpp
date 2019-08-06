/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c default_connector_timeout class using exponential backoff.
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
 #include <cmath> /* pow */
 #include <stdlib.h> /* srand, rand */
 #include <time.h> /* time */

namespace chops {
namespace net {

  /*
   *  @brief Class that uses exponentiql backoff to manage timeouts
   */
class tcp_connector_timeout{
public:
  tcp_connector_timeout((std::chrono::milliseconds initial_timeout, int multiplier = 1, bool reconn=false);
  std::optional<std::chrono::milliseconds> operator()(std::size_t num_connects) {
    int ms = 0;
    //implementation here
    if(!reconn && num_connects > 0){
      return {};
    }

    return std::optional<std::chrono::milliseconds> {ms};
  }

  /*
   * @brief returns number of milliseconds of delay before retry
   *
   * I chose random number generaton for this implementation because
   * deterministic backoff can result in a large number of clients hitting
   * in lockstep.
   */
  int exponential_backoff(int attempts) {
    int max_delay = 60 * 1000;
    int backoff = 0;
    int interval = 5;

    // no need to wait if this is a first attempt
    if ( attempts <= 1) {
      return 0;
    }
    /*
     * after n failed attempts, backoff = x * interval, where
     * x is a number between 0 and 2^n-1
    */
    int limit = pow(2, attempts) - 1;

    srand(time(NULL));
    multiplier = rand() % limit;
    backoff = backoff * multiplier;

    if( backoff > max_delay ) {
      return max_delay;
    }
    return backoff;
  } // end exponential_backoff()
}; // end default_connector_timeout

} // end net namespace
} // end chops namespace

#endif
