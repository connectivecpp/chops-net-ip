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
#include <experimental/executor>

#include <chrono>
#include <system_error>
#include <utility> // std::move, std::forward

namespace chops {

template <typename Clock = std::chrono::steady_clock>
class periodic_timer {
public:

  using duration = typename Clock::duration;
  using time_point = typename Clock::time_point;

private:

  std::experimental::net::basic_waitable_timer<Clock> m_timer;

private:
  template <typename F>
  void handler_impl(F&& f, time_point&& last_tp, duration&& dur, const std::system_error& err) {
    if (!f(err, (Clock::now() - last_tp))) { // pass err and elapsed time to app function obj
      return; // app is finished with timer for now
    }
    m_timer.expires_after(std::move(dur));
    m_timer.async_wait( [f = std::forward(f), dur = std::move(dur), this] (const std::system_error& e) {
        handler_impl(f, Clock::now(), dur, e);
      }
    );
  }

public:

  /**
   * Construct a @c periodic_timer with an @c io_context. Other information such as duration 
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
   * @param ioc @c io_context for asynchronous processing.
   *
   */
  explicit periodic_timer(std::experimental::net::io_context& ioc) : m_timer(ioc) { }

  periodic_timer() = delete; // no default ctor

  // disallow copy construction and copy assignment
  periodic_timer(const periodic_timer&) = delete;
  periodic_timer& operator=(const periodic_timer&) = delete;

  // allow move construction and move assignment
  periodic_timer(periodic_timer&&) = default;
  periodic_timer& operator=(periodic_timer&&) = default;

  // modifying methods

  /**
   * Start the timer, and the application supplied function object will be invoked 
   * after an amount of time specified by the duration parameter.
   *
   * The function object will continue to be invoked as long as it returns @c true.
   *
   * @param f Function object to be invoked. The function object requires a 
   * signature with two parameters, @c const std::system_error& and @c const duration&.
   *
   * @param dur Time duration between timer invocations of the function object.
   */
  template <typename F>
  void start(F&& f, duration&& dur) {
    m_timer.expires_after(std::move(dur));
    m_timer.async_wait( [f = std::forward(f), dur = std::move(dur), this] (const std::system_error& e) {
        handler_impl(f, Clock::now(), dur, e);
      }
    );
  }
  /**
   * Start the timer, and the application supplied function object will be invoked 
   * first at a specified time point, then afterwards as specified by the duration 
   * parameter.
   *
   * @param f Function object to be invoked. The function object requires a 
   * signature with two parameters, @c const std::system_error& and @c const duration&.
   *
   * @param dur Time duration between timer invocations of the function object.
   *
   * @param when Time point when the first timer callback will be invoked.
   */
  template <typename F>
  void start(F&& f, duration&& dur, time_point&& when) {
    m_timer.expires_at(std::move(when));
    m_timer.async_wait( [f = std::forward(f), dur = std::move(dur), this] (const std::system_error& e) {
        handler_impl(f, Clock::now(), dur, e);
      }
    );
  }

  /**
   * Cancel the timer. The application function object will be called with an 
   * "operation aborted" error code.
   *
   * A cancel may implicitly be called if the @c periodic_timer object is move copy 
   * constructed or move assigned.
   */
  void cancel() {
    m_timer.cancel();
  }
};

} // end namespace

#endif

