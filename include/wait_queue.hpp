/** @file
 *
 *  @ingroup utility_module
 *
 *  @brief Multi-reader multi-writer wait queue class for transferring
 *  data between threads.
 *
 *  This utility class allows transferring data between threads with queue 
 *  semantics, using C++ std library general facilities (mutex, condition 
 *  variable). An internal container with queue semantics is created and 
 *  managed within this object. One of the template parameters is the 
 *  container type, allowing customization for specific use cases (see note 
 *  below).
 *
 *  Multiple writer and reader threads can access this object, although when 
 *  a value is pushed, only one reader thread will be notified to consume a 
 *  value.
 *
 *  This class is based on code from the book Concurrency in Action by Anthony 
 *  Williams. The core logic in this class is the same as provided by Anthony 
 *  in his book, but the interfaces have changed and additional features added. 
 *  The name of the utility class template in Anthony's book is @c threadsafe_queue.
 *
 *  @note A fixed size buffer can be used for this utility, which eliminates
 *  queue memory management happening during a push or pop. In particular,
 *  the proposed @c std::ring_span container (C++ 20, most likely) works well 
 *  for this use case, and this code has been tested with "ring-span lite" from 
 *  Martin Moene. 
 *
 *  @note Move semantics are not implemented, not sure what that would mean
 *  for an object that is not meant to be passed around like a value object.
 *  Copy semantics are provided, although making copies of a @c wait_queue
 *  is sensible only in limited situations.
 *
 *  @authors Cliff Green, Anthony Williams
 *  @date 2017
 *
 */

#ifndef WAIT_QUEUE_INCLUDED_H
#define WAIT_QUEUE_INCLUDED_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility> // std::move

namespace chops {

template <typename T, typename Container = std::deque<T> >
class wait_queue {
private:
  mutable std::mutex mut;
  Container data_queue;
  std::condition data_cond;
  bool open = true;

  using lock_guard = std::lock_guard<std::mutex>;

public:

  wait_queue() = default;

  wait_queue(wait_queue&&) = delete;

  wait_queue& operator=(const wait_queue&) = delete;
  wait_queue& operator=(wait_queue&&) = delete;


  /**
   * Copy construct from an existing @c wait_queue.
   *
   * @note Exact type match required, not interested in
   * converting between template parameter types.
   *
   * @param rhs Object to copy from.
   */
  wait_queue(const wait_queue& rhs) : mut(), data_queue(), data_cond(), open(true) {
    // lock other queue for correct threading semantics, then copy data
    lock_guard lk(rhs.mut);
    data_queue = rhs.data_queue;
  }

  bool push(const T& val) {
    if (!open) {
      return false;
    }
    lock_guard lk(mut);
    data_queue.push_back(val);
    data_cond.notify_one();
    return true;
  }

  bool push(T&& val) {
    if (!open) {
      return false;
    }
    lock_guard lk(mut);
    data_queue.push_back(std::move(val));
    data_cond.notify_one();
    return true;
  }

};

} // end namespace

#endif
