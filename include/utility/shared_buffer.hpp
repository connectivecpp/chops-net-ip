/** @file
 *
 *  @ingroup utility_module
 *
 *  @brief Reference counted byte buffer classes, both const and mutable versions.
 *
 *  The @c mutable_shared_buffer and @c const_shared_buffer classes provide byte
 *  oriented buffer classes with internal reference counting. These classes are
 *  used within the Chops Net library to manage data buffer lifetimes, but can also 
 *  be used in application code as utility buffer classes. The @c mutable_shared_buffer 
 *  class can be used to construct a data buffer, and then a @c const_shared_buffer can 
 *  be move constructed from the @c mutable_shared_buffer for use with asynchronous 
 *  library functions (whether Chops Net or C++ Networking TS or Asio). Besides the data 
 *  buffer lifetime management, these utility classes eliminate data buffer copies.
 *
 *  This code is based on and modified from Chris Kohlhoff's Asio example code. It has
 *  been significantly modified by adding a @c mutable_shared_buffer class as well as 
 *  adding convenience methods to the @c shared_const_buffer class.
 *
 *  Example code:
 *  @code
 *    ... fill in example code
 *  @endcode

 *  @authors Cliff Green, Chris Kohlhoff
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SHARED_BUFFER_HPP_INCLUDED
#define SHARED_BUFFER_HPP_INCLUDED

#include <optional>
#include <utility> // std::move, std::move_if_noexcept
#include <type_traits> // for noexcept specs

namespace chops {

template <typename T, typename Container = std::deque<T> >
class wait_queue {
private:
  mutable std::mutex m_mut;
  Container m_data_queue;
  std::condition_variable m_data_cond;
  bool m_closed = false;

  using lock_guard = std::lock_guard<std::mutex>;

public:

  wait_queue() = default;

  /**
   * Construct a @c wait_queue with an iterator range for the container, versus 
   * default construction for the container.
   *
   * This constructor supports a container view type to be used for the internal 
   * container, which require an iterator range as part of the construction. The
   * begin and end iterators must implement the usual C++ iterator range 
   * semantics.
   *
   * The iterators can also be used to initialize a typical container for the 
   * internal buffer (e.g. @c std::deque, the default container type) with starting 
   * data.
   *
   * @param beg Beginning iterator.
   *
   * @param end Ending iterator.
   */
  template <typename Iter>
  wait_queue(Iter beg, Iter end) noexcept : m_mut(), m_data_queue(beg, end), 
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
   * Directly construct an object in the underlying container (using the container's
   * @c emplace_back method) by forwarding the supplied arguments (can be more than one).
   *
   * @param args Arguments to be used in constructing an element at the end of the queue.
   *
   * @note The @c std containers return a reference to the newly constructed element from 
   * @c emplace method calls. @c emplace_push does not follow this convention, and instead 
   * has the same return as the @c push methods.
   *
   * @return @c true if successful, @c false if the @c wait_queue is closed.
   */
  template <typename ... Args>
  bool emplace_push(Args &&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value) {
    lock_guard lk{m_mut};
    if (m_closed) {
      return false;
    }
    m_data_queue.emplace_back(std::forward<Args>(args)...);
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
  std::optional<T> wait_and_pop() noexcept(std::is_nothrow_constructible<T>::value) {
    std::unique_lock<std::mutex> lk{m_mut};
    if (m_closed) {
      return std::optional<T> {};
    }
    m_data_cond.wait ( lk, [this] { return m_closed || !m_data_queue.empty(); } );
    if (m_data_queue.empty()) {
      return std::optional<T> {}; // queue was closed, no data available
    }
    std::optional<T> val {std::move_if_noexcept(m_data_queue.front())}; // move construct if possible
    m_data_queue.pop_front();
    return val;
  }

  /**
   * Pop and return a value from the @c wait_queue if an element is immediately 
   * available, otherwise return an empty @c std::optional.
   *
   * @return A value from the @c wait_queue or an empty @c std::optional if no values are 
   * available in the @c wait_queue.
   */
  std::optional<T> try_pop() noexcept(std::is_nothrow_constructible<T>::value) {
    lock_guard lk{m_mut};
    if (m_data_queue.empty()) {
      return std::optional<T> {};
    }
    std::optional<T> val {std::move_if_noexcept(m_data_queue.front())}; // move construct if possible
    m_data_queue.pop_front();
    return val;
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

