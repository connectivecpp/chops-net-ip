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
 *  An asynchronous timer is more resource-friendly with system resources 
 *  than creating a thread that sleeps. In particular, creating hundreds or 
 *  thousands of timers is very expensive in a "thread per timer" design.
 *
 *  If the application cares about precise (as possible) periodicity, timer 
 *  design must adjust for processing time within the timer callback code 
 *  as well as operating environment imprecision. For example, application 
 *  "wonder app" desires a timer callback to be invoked once every 500 
 *  milliseconds and the callback takes 15 milliseconds to excecute. Also 
 *  occasionally the operating environment takes an extra 10 or 20 milliseconds 
 *  before invoking the callback. Without adjustment the actual interval is 
 *  now 515 milliseconds between callback invocations, with occasional intervals 
 *  up to 535 milliseconds. The @c periodic_timer class adjusts for these 
 *  slippages, up to the precision allowed by the system and environment.
 *
 *  An excellent article on this topic:
 *
 *  https://bulldozer00.com/2013/12/27/periodic-processing-with-standard-c11-facilities/
 *
 *
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
#include <chrono>

namespace chops {

class periodic_timer {
private:
  mutable std::mutex m_mut;
  Container m_data_queue;
  std::condition_variable m_data_cond;
  bool m_closed = false;

  using lock_guard = std::lock_guard<std::mutex>;

public:

  periodic_timer() = delete;

  /**
   * Construct a @c periodic_timer with a duration and an application supplied 
   * funtion object callback.
   *
   * The clock for the asynchronous timer defaults to @c std::chrono::steady_clock.
   * Other clock types can be used if desired (e.g. @c std::chrono::high_resolution_clock 
   * or @c std::chrono::system_clock). Note that some clocks allow time to be externally 
   * adjusted, which may influence the interval between the callback invocation.
   *
   * @param ldksjflk
   *
   * @param end Ending iterator.
   */
  template <typename Clock = std::chrono::steady_clock>
  periodic_timer(
    m_data_cond(), m_closed(false) { }

  // disallow copy or move construction of the entire object
  wait_queue(const wait_queue&) = delete;
  wait_queue(wait_queue&&) = delete;

  // disallow copy or move assigment of the entire object
  wait_queue& operator=(const wait_queue&) = delete;
  wait_queue& operator=(wait_queue&&) = delete;

  // modifying methods

  /**
   * Open a previously closed @c wait_queue for processing.
   *
   * @note The initial state of a @c wait_queue is open.
   */
  void open() noexcept {
    lock_guard lk{m_mut};
    m_closed = false;
  }

  /**
   * Close a @c wait_queue for processing. All waiting reader threaders will be 
   * notified. Subsequent @c push operations will return @c false.
   */
  void close() noexcept {
    lock_guard lk{m_mut};
    m_closed = true;
    m_data_cond.notify_all();
  }

  /**
   * Push a value, by copying, to the @c wait_queue. A waiting reader thread (if any) 
   * will be notified that a value has been added.
   *
   * @param val Val to copy into the queue.
   *
   * @return @c true if successful, @c false if the @c wait_queue is closed.
   */
  bool push(const T& val) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    lock_guard lk{m_mut};
    if (m_closed) {
      return false;
    }
    m_data_queue.push_back(val);
    m_data_cond.notify_one();
    return true;
  }

  /**
   * This method has the same semantics as the other @c push, except that the value will 
   * be moved (if possible) instead of copied.
   */
  bool push(T&& val) noexcept(std::is_nothrow_move_constructible<T>::value) {
    lock_guard lk{m_mut};
    if (m_closed) {
      return false;
    }
    m_data_queue.push_back(std::move(val));
    m_data_cond.notify_one();
    return true;
  }

  /**
   * Pop and return a value from the @c wait_queue, blocking and waiting for a writer thread to 
   * push a value if one is not immediately available.
   *
   * If this method is called after a @c wait_queue has been closed, an empty @c std::optional 
   * is returned. If a @c wait_queue needs to be flushed after it is closed, @c try_pop should 
   * be called instead.
   *
   * @return A value from the @c wait_queue (if non-empty). If the @c std::optional is empty, 
   * the @c wait_queue has been closed.
   */
  std::optional<T> wait_and_pop() noexcept(std::is_nothrow_move_constructible<T>::value || 
                                           std::is_nothrow_copy_constructible<T>::value) {
    std::unique_lock<std::mutex> lk{m_mut};
    if (m_closed) {
      return std::optional<T> {};
    }
    m_data_cond.wait ( lk, [this] { return m_closed || !m_data_queue.empty(); } );
    if (m_data_queue.empty()) {
      return std::optional<T> {}; // queue was closed, no data available
    }
    if constexpr (std::is_move_constructible<T>::value) {
      std::optional<T> val {std::move(m_data_queue.front())}; // move ctor if possible
      m_data_queue.pop_front();
      return val;
    }
    else {
      std::optional<T> val {m_data_queue.front()}; // copy ctor instead of move ctor
      m_data_queue.pop_front();
      return val;
    }
  }

  /**
   * Pop and return a value from the @c wait_queue if an element is immediately 
   * available, otherwise return an empty @c std::optional.
   *
   * @return A value from the @c wait_queue or an empty @c std::optional if no values are 
   * available in the @c wait_queue.
   */
  std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible<T>::value || 
                                      std::is_nothrow_copy_constructible<T>::value) {
    lock_guard lk{m_mut};
    if (m_data_queue.empty()) {
      return std::optional<T> {};
    }
    if constexpr (std::is_move_constructible<T>::value) {
      std::optional<T> val {std::move(m_data_queue.front())}; // move ctor if possible
      m_data_queue.pop_front();
      return val;
    }
    else {
      std::optional<T> val {m_data_queue.front()}; // copy ctor instead of move ctor
      m_data_queue.pop_front();
      return val;
    }
  }

  // non-modifying methods

  /**
   * Apply a non-modifying function object to all elements of the queue.
   *
   * The function object is not allowed to modify any of the elements. 
   * The supplied function object is passed a const reference to the element 
   * type.
   *
   * This method can be used when an iteration of the elements is needed,
   * such as to print the elements, or copy them to another container, or 
   * to interrogate values of the elements.
   *
   * @note The entire @c wait_queue is locked while @c apply is in process, 
   * so passing in a function object that blocks or takes a lot of processing 
   * time may result in slow performance.
   *
   * @note It is undefined behavior if the function object calls into the 
   * same @c wait_queue since it results in recursive mutex locks.
   */
  template <typename F>
  void apply(F&& f) const noexcept(std::is_nothrow_invocable<F&&, const T&>::value) {
    lock_guard lk{m_mut};
    for (const T& elem : m_data_queue) {
      f(elem);
    }
  }

  /**
   * Query whether the @ close method has been called on the @c wait_queue.
   *
   * @return @c true if the @c wait_queue has been closed.
   */
  bool is_closed() const noexcept {
    lock_guard lk{m_mut};
    return m_closed;
  }

  /**
   * Query whether the @c wait_queue is empty or not.
   *
   * @return @c true if the @c wait_queue is empty.
   */
  bool empty() const noexcept {
    lock_guard lk{m_mut};
    return m_data_queue.empty();
  }

  using size_type = typename Container::size_type;

  /**
   * Get the number of elements in the @c wait_queue.
   *
   * @return Number of elements in the @c wait_queue.
   */
  size_type size() const noexcept {
    lock_guard lk{m_mut};
    return m_data_queue.size();
  }

};

} // end namespace

#endif

