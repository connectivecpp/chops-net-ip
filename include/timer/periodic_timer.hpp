/** @file
 *
 *  @ingroup timer_module
 *
 *  @brief An asynchronous periodic timer that adjusts for time jitter.
 *
 *  Writing code using asynchronous timers from the C++ Networking Technical 
 *  Specification (TS) is relatively easy. However, there are no timers that 
 *  are periodic. This class simplifies the task, using application supplied 
 *  function object callbacks.
 *
 *  A @c periodic_timer stops when the application supplied function object 
 *  returns @c false rather than @c true.
 *
 *  A periodic timer can be used as a "one-shot" timer by finishing after 
 *  one invocation (i.e. unconditionally return @c false from the function 
 *  object).
 *
 *  Example usage:
 *
 *  @code
 *    // fill in here
 *  @endcode
 *
 *  @author Cliff Green
 *  @date 2017
 *
 */

#ifndef PERIODIC_TIMER_INCLUDED_H
#define PERIODIC_TIMER_INCLUDED_H

#include <experimental/timer>
#include <experimental/io_context>

#include <chrono>
#include <system_error>
#include <utility> // std::move

namespace chops {

namespace details {
}

template <typename Clock = std::chrono::steady_clock>
class periodic_timer {
public:

  using duration = typename Clock::duration;
  using time_point = typename Clock::time_point;

private:

  std::experimental::basic_waitable_timer<Clock> m_timer;
  duration m_duration;
  time_point m_last_tp;

private:
  template <typename F>
  void handler_impl(F&& f, const std::system_error& err) {
    duration elapsed { Clock::now() - m_last_tp };
    if (!f(err, elapsed)) {
      return;
    }
    m_timer.expires_after(m_duration);
    m_last_tp = Clock::now();
    m_timer.async_wait(

  }

public:

  /**
   * Construct a @c periodic_timer with an @c io_context and time duration. Other information 
   * will be supplied when @c start is called.
   *
   * Constructing a @c periodic_timer does not start the actual timer. Calling the @c start 
   * method starts the timer.
   *
   * The clock for the asynchronous timer defaults to @c std::chrono::steady_clock.
   * Other clock types can be used if desired (e.g. @c std::chrono::high_resolution_clock 
   * or @c std::chrono::system_clock). Note that some clocks allow time to be externally 
   * adjusted, which may influence the interval between the callback invocation.
   *
   * Move semantics are allowed for this type, but not copy semantics. When a move 
   * construction or move assignment completes, all timers are cancelled with 
   * appropriate errors, and @c start will need to be called.
   *
   * @param ldksjflk
   *
   * @param 
   */
  template <typename Clock = std::chrono::steady_clock>
  explicit periodic_timer(std::experimental::io_context& ioc, duration&& dur) :
      m_timer(ioc), std::move(dur), m_last_tp() { }

  periodic_timer() = delete; // no default ctor

  // disallow copy construction and copy assignment
  periodic_timer(const wait_queue&) = delete;
  periodic_timer& operator=(const wait_queue&) = delete;

  // allow move construction and move assignment
  periodic_timer(periodic_timer&&) = default;
  periodic_timer& operator=(periodic_timer&&) = default;

  // modifying methods

  /**
   */
  template <typename F>
  void start(F&& f) {
    m_timer.expires_after(m_duration);
    m_last_tp = Clock::now();
    m_timer.async_wait ([f, this] mutable (const std::error& err) {
        if (f(err)) {
          
        }
      };
    );
  }
  template <typename F>
  void start(F&& f, time_point&& when) {
    m_timer.expires_at(when);
    // set m_last_tp accordingly
    m_last_tp = Clock::now() - m_duration;
    m_timer.async_wait (sdlkfj);
  }

  // non-modifying methods

};

} // end namespace

#endif

