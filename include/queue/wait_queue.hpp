/** @file
 *
 *  @defgroup queue_module Class and functions for Chops thread queue functionality.
 *
 *  @ingroup queue_module
 *
 *  @brief Multi-reader multi-writer wait queue class for transferring
 *  data between threads.
 *
 *  This class allows transferring data between threads with queue semantics
 *  (push, pop), using C++ std library general facilities (mutex, condition 
 *  variable). An internal container is managed within this class. 
 *
 *  Multiple writer and reader threads can access this object, although when 
 *  a value is pushed, only one reader thread will be notified to consume a 
 *  value.
 *
 *  One of the template parameters is the container type, allowing customization 
 *  for specific use cases (see below for additional details).
 *
 *  If the @c close method is called, all reader threads calling @c wait_and_pop 
 *  are notified, and an empty value returned to those threads. Subsequent calls 
 *  to @c push will return a @c false value.
 *
 *  Example usage, default container:
 *
 *  @code
 *    chops::wait_queue<int> wq;
 *
 *    // inside writer thread, assume wq passed in by reference
 *    wq.push(42);
 *    ...
 *    // all finished, time to shutdown
 *    wq.close();
 *
 *    // inside reader thread, assume wq passed in by reference
 *    auto rtn_val = wq.wait_and_pop(); // return type is std::optional<int>
 *    if (!rtn_val) { // empty value, close has been called
 *      // time to end reader thread
 *    }
 *    if (*rtn_val == 42) ...
 *  @endcode
 *
 *  Example usage with ring buffer (from Martin Moene):
 *
 *  @code
 *    const int sz = 20;
 *    int buf[sz];
 *    chops::wait_queue<int, nonstd::ring_span<int> > wq(buf+0, buf+sz);
 *    // push and pop same as code with default container
 *  @endcode
 *
 *  The container type must support the following (depending on which 
 *  methods are instantiated): default construction, construction from a 
 *  begin and end iterator, construction with an initial size, 
 *  @c push_back (preferably overloaded for both copy and move), 
 *  @c emplace_back (with a template parameter pack), @c front, @c pop_front, 
 *  @c empty, and @c size. The container must also have a @c size_type
 *  defined.
 *
 *  This class is based on code from the book Concurrency in Action by Anthony 
 *  Williams. The core logic in this class is the same as provided by Anthony 
 *  in his book, but the interfaces have changed and additional features added. 
 *  The name of the utility class template in Anthony's book is @c threadsafe_queue.
 *
 *  @note A fixed size buffer can be used for the container, which eliminates
 *  queue memory management happening during a @c push or @c pop. In particular,
 *  the proposed @c std::ring_span container (C++ 20, most likely) works well 
 *  for this use case, and this code has been tested with @c ring-span lite from 
 *  Martin Moene. The constructor that takes an iterator range can be used for 
 *  a container view type, which means that the @c wait_queue owns and manages 
 *  a view rather than the underlying container buffer.
 *
 *  @note The @c boost @c circular_buffer can be used for the container type. Memory is
 *  allocated only once, at container construction time. This may be useful for
 *  environments where construction can be dynamic, but a @c push or @c pop must
 *  not allocate or deallocate memory. 
 *
 *  @note If the container type is @c boost @c circular_buffer then the default
 *  constructor for @c wait_queue cannot be used (since it would result in a container
 *  with an empty capacity).
 *
 *  @note Iterators are not supported, due to obvious difficulties with maintaining 
 *  consistency and integrity. The @c apply method can be used to access the internal
 *  data in a threadsafe manner.
 *
 *  @note Copy and move construction or assignment for the whole queue is
 *  disallowed, since the use cases and underlying implications are not clear 
 *  for those operations. In particular, the exception implications for 
 *  assigning the internal data from one queue to another is messy, and the general 
 *  semantics of what it means is not clearly defined. If there is data in one 
 *  @c wait_queue that must be copied or moved to another, the @c apply method can 
 *  be used or individual @c push and @c pop methods called, even if not as efficient 
 *  as an internal copy or move.
 *
 *  @note Very few methods are declared as @c noexcept since very few of the @c std::mutex,
 *  @c std::condition_variable, and @c std::lock_guard methods are @c noexcept.
 *
 *  @authors Cliff Green, Anthony Williams
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef WAIT_QUEUE_HPP_INCLUDED
#define WAIT_QUEUE_HPP_INCLUDED

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <utility> // std::move, std::move_if_noexcept
#include <type_traits> // for noexcept specs

namespace chops {

template <typename T, typename Container = std::deque<T> >
class wait_queue {
private:
  mutable std::mutex m_mut;
  std::condition_variable m_data_cond;
  std::atomic_bool m_closed;
  Container m_data_queue;

  using lock_guard = std::lock_guard<std::mutex>;

public:

  using size_type = typename Container::size_type;

public:

  /**
   * @brief Default construct a @c wait_queue.
   *
   * @note A default constructed @c boost @c circular_buffer cannot do
   * anything, so a different @c wait_queue constructor must be used if
   * instantiated with a @c boost @c circular_buffer.
   *
   */
  wait_queue()
    // noexcept(std::is_nothrow_constructible<Container>::value)
      : m_mut(), m_data_cond(), m_closed(false), m_data_queue() { }

  /**
   * @brief Construct a @c wait_queue with an iterator range for the container.
   *
   * Construct the container (or container view) with an iterator range. Whether
   * element copies are performed depends on the container type. Most container
   * types copy initial elements as defined by the range and the initial size is
   * set accordingly. A @c ring_span, however, uses the range distance to define 
   * a capacity and sets the initial size to zero.
   *
   * @note This is the only constructor that can be used with a @c ring_span
   * container type.
   *
   * @param beg Beginning iterator.
   *
   * @param end Ending iterator.
   */
  template <typename Iter>
  wait_queue(Iter beg, Iter end)
    // noexcept(std::is_nothrow_constructible<Container, Iter, Iter>::value)
      : m_mut(), m_data_cond(), m_closed(false), m_data_queue(beg, end) { }

  /**
   * @brief Construct a @c wait_queue with an initial size or capacity.
   *
   * Construct the container (or container view) with an initial size of default
   * inserted elements or with an initial capacity, depending on the container type.
   *
   * @note This constructor cannot be used with a @c ring_span container type.
   * 
   * @note Using this constructor with a @c boost @c circular_buffer creates a
   * container with the specified capacity, but an initial empty size.
   *
   * @note Using this constructor with most standard library container types 
   * creates a container initialized with default inserted elements.
   *
   * @param sz Capacity or initial size, depending on container type.
   *
   */
  wait_queue(size_type sz)
    // noexcept(std::is_nothrow_constructible<Container, size_type>::value)
      : m_mut(), m_data_cond(), m_closed(false), m_data_queue(sz) { }

  // disallow copy or move construction of the entire object
  wait_queue(const wait_queue&) = delete;
  wait_queue(wait_queue&&) = delete;

  // disallow copy or move assigment of the entire object
  wait_queue& operator=(const wait_queue&) = delete;
  wait_queue& operator=(wait_queue&&) = delete;

  // modifying methods

  /**
   * @brief Open a previously closed @c wait_queue for processing.
   *
   * @note The initial state of a @c wait_queue is open.
   */
  void open() noexcept {
    m_closed = false;
  }

  /**
   * @brief Close a @c wait_queue for processing.
   *
   * When this method is called, all waiting reader threaders will be notified. 
   * Subsequent @c push operations will return @c false.
   */
  void close() /* noexcept */ {
    m_closed = true;
    lock_guard lk{m_mut};
    m_data_cond.notify_all();
  }

  /**
   * @brief Push a value, by copying, to the @c wait_queue.
   *
   * When a value is pushed, one waiting reader thread (if any) will be 
   * notified that a value has been added.
   *
   * @param val Val to copy into the queue.
   *
   * @return @c true if successful, @c false if the @c wait_queue is closed.
   */
  bool push(const T& val) /* noexcept(std::is_nothrow_copy_constructible<T>::value) */ {
    if (m_closed) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.push_back(val);
    m_data_cond.notify_one();
    return true;
  }

  /**
   * @brief Push a value, either by moving or copying, to the @c wait_queue.
   *
   * This method has the same semantics as the other @c push, except that the value will 
   * be moved (if possible) instead of copied.
   */
  bool push(T&& val) /* noexcept(std::is_nothrow_move_constructible<T>::value) */ {
    if (m_closed) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.push_back(std::move(val));
    m_data_cond.notify_one();
    return true;
  }

  /**
   * @brief Directly construct an object in the underlying container (using the container's
   * @c emplace_back method) by forwarding the supplied arguments (can be more than one).
   *
   * @param args Arguments to be used in constructing an element at the end of the queue.
   *
   * @note The @c std containers return a reference to the newly constructed element from 
   * @c emplace method calls. @c emplace_push for a @c wait_queue does not follow this 
   * convention and instead has the same return as the @c push methods.
   *
   * @return @c true if successful, @c false if the @c wait_queue is closed.
   */
  template <typename ... Args>
  bool emplace_push(Args &&... args) /* noexcept(std::is_nothrow_constructible<T, Args...>::value)*/ {
    if (m_closed) {
      return false;
    }
    lock_guard lk{m_mut};
    m_data_queue.emplace_back(std::forward<Args>(args)...);
    m_data_cond.notify_one();
    return true;
  }

  /**
   * @brief Pop and return a value from the @c wait_queue, blocking and waiting for a writer 
   * thread to push a value if one is not immediately available.
   *
   * If this method is called after a @c wait_queue has been closed, an empty @c std::optional 
   * is returned. If a @c wait_queue needs to be flushed after it is closed, @c try_pop should 
   * be called instead.
   *
   * @return A value from the @c wait_queue (if non-empty). If the @c std::optional is empty, 
   * the @c wait_queue has been closed.
   */
  std::optional<T> wait_and_pop() /* noexcept(std::is_nothrow_constructible<T>::value) */ {
    if (m_closed) {
      return std::optional<T> {};
    }
    std::unique_lock<std::mutex> lk{m_mut};
    m_data_cond.wait ( lk, [this] { return m_closed || !m_data_queue.empty(); } );
    if (m_data_queue.empty()) {
      return std::optional<T> {}; // queue was closed, no data available
    }
    std::optional<T> val {std::move_if_noexcept(m_data_queue.front())}; // move construct if possible
    m_data_queue.pop_front();
    return val;
  }

  /**
   * @brief Pop and return a value from the @c wait_queue if an element is immediately 
   * available, otherwise return an empty @c std::optional.
   *
   * @return A value from the @c wait_queue or an empty @c std::optional if no values are 
   * available in the @c wait_queue.
   */
  std::optional<T> try_pop() /* noexcept(std::is_nothrow_constructible<T>::value) */ {
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
   * @brief Apply a non-modifying function object to all elements of the queue.
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
  void apply(F&& f) const /* noexcept(std::is_nothrow_invocable<F&&, const T&>::value) */ {
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
    return m_closed;
  }

  /**
   * Query whether the @c wait_queue is empty or not.
   *
   * @return @c true if the @c wait_queue is empty.
   */
  bool empty() const /* noexcept */ {
    lock_guard lk{m_mut};
    return m_data_queue.empty();
  }

  /**
   * Get the number of elements in the @c wait_queue.
   *
   * @return Number of elements in the @c wait_queue.
   */
  size_type size() const /* noexcept */ {
    lock_guard lk{m_mut};
    return m_data_queue.size();
  }

};

} // end namespace

#endif

