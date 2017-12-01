/** @file
 *
 *  @ingroup timer_module
 *
 *  @brief An asynchronous periodic timer providing both duration and timepoint 
 *  based periods.
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
 *  @note @c std::chrono facilities seem to be underspecified on @c noexcept,
 *  very few of the functions in @c periodic_timer are @c noexcept.
 *
 *  @author Cliff Green
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef PERIODIC_TIMER_HPP_INCLUDED
#define PERIODIC_TIMER_HPP_INCLUDED

#include <experimental/timer> // Networking TS include
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
  void duration_handler_impl(F&& f, const time_point& last_tp, const duration& dur, const std::error_code& err) {
    time_point now_time { Clock::now() };
    // pass err and elapsed time to app function obj
    if (!f(err, now_time - last_tp) || err == std::experimental::net::error::operation_aborted) {
      return; // app is finished with timer for now or timer was cancelled
    }
    m_timer.expires_after(dur);
    m_timer.async_wait( [f = std::forward<F>(f), now_time, dur, this]
            (const std::error_code& e) {
        duration_handler_impl(f, now_time, dur, e);
      }
    );
  }
  template <typename F>
  void timepoint_handler_impl(F&& f, const time_point& last_tp, const duration& dur, const std::error_code& err) {
    // pass err and elapsed time to app function obj
    if (!f(err, (Clock::now() - last_tp)) || err == std::experimental::net::error::operation_aborted) {
      return; // app is finished with timer for now or timer was cancelled
    }
    m_timer.expires_at(last_tp + dur + dur);
    m_timer.async_wait( [f = std::forward<F>(f), last_tp, dur, this]
            (const std::error_code& e) {
        timepoint_handler_impl(f, (last_tp+dur), dur, e);
      }
    );
  }

public:

  /**
   * Construct a @c periodic_timer with an @c io_context. Other information such as duration 
   * will be supplied when @c start is called.
   *
   * Constructing a @c periodic_timer does not start the actual timer. Calling one of the 
   * @c start methods starts the timer.
   *
   * The application supplied function object requires a signature with two parameters, 
   * @c const @c std::error_code& and @c const @c duration&. The second parameter provides an 
   * elapsed time from the previous callback.
   *
   * The clock for the asynchronous timer defaults to @c std::chrono::steady_clock.
   * Other clock types can be used if desired (e.g. @c std::chrono::high_resolution_clock 
   * or @c std::chrono::system_clock). Note that some clocks allow time to be externally 
   * adjusted, which may influence the interval between the callback invocation.
   *
   * Move semantics are allowed for this type, but not copy semantics. When a move 
   * construction or move assignment completes, all timers are cancelled with 
   * appropriate notification, and @c start will need to be called.
   *
   * @param ioc @c io_context for asynchronous processing.
   *
   */
  explicit periodic_timer(std::experimental::net::io_context& ioc) noexcept : m_timer(ioc) { }

  periodic_timer() = delete; // no default ctor

  // disallow copy construction and copy assignment
  periodic_timer(const periodic_timer&) = delete;
  periodic_timer& operator=(const periodic_timer&) = delete;

  // allow move construction and move assignment
  periodic_timer(periodic_timer&&) = default;
  periodic_timer& operator=(periodic_timer&& rhs) {
    m_timer.cancel();
    m_timer = std::move(rhs.m_timer);
  }

  // modifying methods

  /**
   * Start the timer, and the application supplied function object will be invoked 
   * after an amount of time specified by the duration parameter.
   *
   * The function object will continue to be invoked as long as it returns @c true.
   *
   * @param f Function object to be invoked. 
   *
   * @param dur Interval to be used between callback invocations.
   */
  template <typename F>
  void start_duration_timer(F&& f, const duration& dur) {
    m_timer.expires_after(dur);
    m_timer.async_wait( [f = std::forward<F>(f), dur, this] (const std::error_code& e) {
        duration_handler_impl(f, Clock::now(), dur, e);
      }
    );
  }
  /**
   * Start the timer, and the application supplied function object will be invoked 
   * first at a specified time point, then afterwards as specified by the duration 
   * parameter.
   *
   * The function object will continue to be invoked as long as it returns @c true.
   *
   * @param f Function object to be invoked.
   *
   * @param dur Interval to be used between callback invocations.
   *
   * @param when Time point when the first timer callback will be invoked.
   */
  template <typename F>
  void start_duration_timer(F&& f, const duration& dur, const time_point& when) {
    m_timer.expires_at(when);
    m_timer.async_wait( [f = std::forward<F>(f), dur, this] (const std::error_code& e) {
        duration_handler_impl(f, Clock::now(), dur, e);
      }
    );
  }
  /**
   * Start the timer, and the application supplied function object will be invoked 
   * on timepoints with an interval specified by the duration.
   *
   * The function object will continue to be invoked as long as it returns @c true.
   *
   * @param f Function object to be invoked. 
   *
   * @param dur Interval to be used between callback invocations.
   */
  template <typename F>
  void start_timepoint_timer(F&& f, const duration& dur) {
    start_timepoint_timer(std::forward<F>(f), dur, (Clock::now() + dur));
  }
  /**
   * Start the timer on the specified timepoint, and the application supplied function object 
   * will be invoked on timepoints with an interval specified by the duration.
   *
   * The function object will continue to be invoked as long as it returns @c true.
   *
   * @param f Function object to be invoked. 
   *
   * @param dur Interval to be used between callback invocations.
   *
   * @param when Time point when the first timer callback will be invoked.
   *
   * @note The elapsed time for the first callback invocation is artificially set to the 
   * duration interval.
   */
  template <typename F>
  void start_timepoint_timer(F&& f, const duration& dur, const time_point& when) {
    m_timer.expires_at(when);
    m_timer.async_wait( [f = std::forward<F>(f), when, dur, this]
            (const std::error_code& e) {
        timepoint_handler_impl(f, (when-dur), dur, e);
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

