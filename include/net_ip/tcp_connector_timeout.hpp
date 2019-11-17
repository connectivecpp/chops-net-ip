/** @file
 *
 *  @ingroup net_ip_module
 *
 *  @brief Classes that implement a connect timeout function object interface for the 
 *  @c tcp_connector detail class.
 *
 *  @author Nathan Deutsch
 *
 *  All classes implement a functor with the following interface:
 *  @code
 *  std::optional<std::chrono::milliseconds> operator()(std::size_t connect_attempts);
 *  @endcode
 *
 *  The return value is the number of milliseconds for the TCP connector timeout, if present. If
 *  the value is not present, no further TCP connects are attempted. 
 *
 *  The @c connect_attempts parameter is the number of connect attempts made by the TCP connector. 
 *  It will always be greater than zero.
 *
 *  The following use cases are supported: 1) always return the same timeout (i.e. no scale factor, no 
 *  backoff); 2) scale the timeout by a multiplier or exponential factor for each connect attempt, cap the 
 *  timeout at a max timeout value; 3) stop after N connect attempts.
 *
 *  Other use cases can be implemented by applications providing a function object or lambda in the 
 *  @c make_tcp_connector method call.
 *
 *  The TCP connector uses a copy of the initial timeout function object when connection attempts are started.
 *  In other words, if a TCP connection is brought down due to a network error and the "re-connect on error"
 *  flag is set @c true in the @c make_tcp_connector call, then the timeout function object will start in 
 *  the initial state (as supplied). This may make a difference for function objects that need to store
 *  store state.
 *
 *  Copyright (c) 2019 by Cliff Green, Nathan Deutsch
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED
#define TCP_CONNECTOR_TIMEOUT_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <cmath> // std::pow
#include <chrono>
#include <optional>

namespace chops {
namespace net {

/*
 * @brief A @c simple_timeout always returns the same timeout value.
 *
 */
struct simple_timeout {

  const std::chrono::milliseconds m_timeout;
  using opt_ms = std::optional<std::chrono::milliseconds>;

/*
 * @brief Construct a @c simple_timeout.
 *
 * @param timeout Milliseconds to wait between unsuccessful connect attempts.
 */
  simple_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) : m_timeout(timeout) {}

/*
 *  @brief Function call interface, for use by TCP connector functionality.
 *
 *  This method is not directly called by application code. It is called by the TCP connector
 *  internal functionality. See file documentation for documentation on parameters and return
 *  value.
 */
  opt_ms operator()(std::size_t) const noexcept {
    return opt_ms { m_timeout };
  }
};

/*
 * @brief A @c counted_timeout limits the number of connect attempts.
 *
 */
struct counted_timeout {

  const std::chrono::milliseconds m_timeout;
  const std::size_t m_max_attempts;
  using opt_ms = std::optional<std::chrono::milliseconds>;

/*
 * @brief Construct a @c counted_timeout.
 *
 * @param timeout Milliseconds to wait between unsuccessful connect attempts.
 *
 * @param max_conn_attempts Maximum number of connect attempts.
 */
  counted_timeout (std::chrono::milliseconds timeout, std::size_t max_conn_attempts) :
    m_timeout(timeout), m_max_attempts(max_conn_attempts) {}

/*
 *  @brief Function call interface, for use by TCP connector functionality.
 *
 *  This method is not directly called by application code. It is called by the TCP connector
 *  internal functionality. See file documentation for documentation on parameters and return
 *  value.
 */
  opt_ms operator()(std::size_t attempts) const noexcept {
    return (attempts > m_max_attempts) ? opt_ms { } : opt_ms { m_timeout };
  }
};

/*
 *  @brief Increase the timeout value by a scaling factor up to a maximum value.
 *
 *  Increase the timeout value for each unsuccessful connect attempt. This decreases network
 *  traffic where there are multiple connectors all trying to connect to an unreachable host.
 *
 */
struct backoff_timeout {

  using tick_type = typename std::chrono::milliseconds::rep;
  const tick_type m_initial_ticks;
  const tick_type m_max_ticks;
  const int m_scale_factor;
  using opt_ms = std::optional<std::chrono::milliseconds>;

/*
 * @brief Construct a @c backoff_timeout.
 *
 * @param initial_timeout Milliseconds to wait for the first connect attempt.
 *
 * @param max_timeout Maximum timeout value.
 *
 * @param scale_factor The timeout value is multiplied by this number for each connect attempt.
 */
  backoff_timeout(std::chrono::milliseconds initial_timeout, std::chrono::milliseconds max_timeout,
                  int scale_factor = 2) :
    m_initial_ticks(initial_timeout.count()), m_max_ticks(max_timeout.count()),
    m_scale_factor(scale_factor) { }

/*
 *  @brief Function call interface, for use by TCP connector functionality.
 *
 *  This method is not directly called by application code. It is called by the TCP connector
 *  internal functionality. See file documentation for documentation on parameters and return
 *  value.
 */
  opt_ms operator()(std::size_t attempts) const noexcept {
    auto tmp = static_cast<tick_type>((attempts-1u) * m_scale_factor * m_initial_ticks);
    tmp = (tmp == 0u ? m_initial_ticks : ((tmp > m_max_ticks) ? m_max_ticks : tmp));
    return opt_ms { std::chrono::milliseconds { tmp } };
  }
};

/*
 *  @brief Exponentially increase the timeout value up to a maximum.
 *
 *  Increase the timeout value similar to @c backoff_timeout, except using an exponential
 *  @c std::pow calculation instead of a scaled backoff.
 *
 */
struct exponential_backoff_timeout {

  using tick_type = typename std::chrono::milliseconds::rep;
  const tick_type m_initial_ticks;
  const tick_type m_max_ticks;
  using opt_ms = std::optional<std::chrono::milliseconds>;

/*
 * @brief Construct a @c exponential_backoff_timeout.
 *
 * @param initial_timeout Milliseconds to wait for the first connect attempt.
 *
 * @param max_timeout Maximum timeout value.
 *
 */
  exponential_backoff_timeout(std::chrono::milliseconds initial_timeout, 
                              std::chrono::milliseconds max_timeout) :
    m_initial_ticks(initial_timeout.count()), m_max_ticks(max_timeout.count()) { }

/*
 *  @brief Function call interface, for use by TCP connector functionality.
 *
 *  This method is not directly called by application code. It is called by the TCP connector
 *  internal functionality. See file documentation for documentation on parameters and return
 *  value.
 */
  opt_ms operator()(std::size_t attempts) const noexcept {

    auto tmp = static_cast<tick_type>(std::pow(m_initial_ticks, attempts));
    std::chrono::milliseconds tout { (tmp > m_max_ticks) ? m_max_ticks : tmp };
    return opt_ms { tout };
  } //end operator()

};

} // end net namespace
} // end chops namespace

#endif

