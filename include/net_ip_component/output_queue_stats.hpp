/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that collect and deliver @c output_queue_stats from a 
 *  sequence.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef OUTPUT_QUEUE_STATS_HPP_INCLUDED
#define OUTPUT_QUEUE_STATS_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <numeric> // std::accumulate

#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {

/**
 *  @brief Accumulate @c output_queue_stats given a sequence of
 *  @c basic_io_output objects.
 *
 *  The @c basic_io_output object can be of either @c tcp_io_output or
 *  @c udp_io_output types.
 *
 *  @param beg Beginning iterator of sequence of @c basic_io_output
 *  objects.
 *
 *  @param end Ending iterator of sequence.
 *
 *  @return @c output_queue_stats containing accumulated statistics.
 */
template <typename Iter>
output_queue_stats accumulate_output_queue_stats(Iter beg, Iter end) {
  return std::accumulate(beg, end, output_queue_stats(),
			  [] (const output_queue_stats& sum, const auto& io) {
          auto rhs = io.get_output_queue_stats();
          return output_queue_stats(sum.output_queue_size + rhs.output_queue_size,
                                    sum.bytes_in_output_queue + rhs.bytes_in_output_queue);
    }
  );
}

/**
 *  @brief Accumulate @c output_queue_stats on a sequence of
 *  @c basic_io_output objects until a condition is satisfied.
 *
 *  Given a sequence of @c basic_io_output objects, accumulate statistics
 *  until a supplied condition function object is satisfied.
 *
 *  The condition object typically checks for a specific count of 0 or similar.
 *  It must have a signature of:
 *  @code
 *    bool (const chops::net::output_queue_stats&)
 *  @endcode
 *
 *  It is highly recommended that a sleep or other blocking operation is performed
 *  when the condition returns @c false, otherwise a tight processing loop will
 *  occur.
 *
 *  For example:
 *  @code
 *  bool check_stats (const chops::net::output_queue_stats& st) {
 *    if (st.output_queue_size == 0u) {
 *      return true;
 *    }
 *    std::this_thread::sleep_for(std::chrono::milliseconds(200));
 *    return false;
 *  }
 *  // above code could also be in a lambda
 *  accumulate_output_queue_stats_until (io_vec.cbegin(), io_vec.cend(),
 *                                       check_stats);
 *  @endcode
 *
 *  @param beg Beginning iterator of sequence of @c basic_io_output
 *  objects.
 *
 *  @param end Ending iterator of sequence.
 *
 *  @param cond Condition function object invoked after each accumulation,
 *  returning @c true causes accumulation loop to finish. 
 *
 */
template <typename Iter, typename Cond>
void accumulate_output_queue_stats_until(Iter beg, Iter end, Cond&& cond) {
  while (cond(accumulate_output_queue_stats(beg, end)) {
    ; // no-op, tight loop
  }
}

/**
 *  @brief Accumulate @c output_queue_stats given a sequence of
 *  @c net_entity objects, using the @c visit_io_output method on each
 *  @c net_entity.
 *
 *  @tparam IOT Either @c chops::net::tcp_io or @c chops::net::udp_io.
 *
 *  @param beg Beginning iterator of sequence of @c net_entity
 *  objects.
 *
 *  @param end Ending iterator of sequence.
 *
 *  @return @c output_queue_stats containing accumulated statistics.
 *  
 */
template <typename IOT, typename Iter>
output_queue_stats accumulate_net_entity_output_queue_stats(Iter beg, Iter end) {
  return std::accumulate(beg, end, output_queue_stats(),
			  [] (const output_queue_stats& sum, const auto& ne) {
          output_queue_stats st{};
          ne.visit_io_output([&st] (basic_io_output<IOT> io) {
              st.output_queue_size += io.output_queue_size;
              st.bytes_in_output_queue += io.bytes_in_output_queue;
            }
          );
          return output_queue_stats(sum.output_queue_size + st.output_queue_size,
                                    sum.bytes_in_output_queue + st.bytes_in_output_queue);
    }
  );
}

/**
 *  @brief Accumulate @c output_queue_stats on a sequence of
 *  @c net_entity objects until a condition is satisfied.
 *
 *  Given a sequence of @c net_entity objects, accumulate statistics
 *  until a supplied condition function object is satisfied.
 *
 *  @param beg Beginning iterator of sequence of @c net_entity
 *  objects.
 *
 *  @param end Ending iterator of sequence.
 *
 *  @param cond Condition function object invoked after each accumulation,
 *  returning @c true causes accumulation loop to finish. 
 *
 */
template <typename IOT, typename Iter, typename Cond>
void accumulate_net_entity_output_queue_stats_until(Iter beg, Iter end, Cond&& cond) {
  while (cond(accumulate_net_entity_output_queue_stats(beg, end)) {
    ; // no-op, tight loop
  }
}

} // end net namespace
} // end chops namespace

#endif

