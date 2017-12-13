/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Structures containing statistics gathered on internal queues.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef QUEUE_STATS_HPP_INCLUDED
#define QUEUE_STATS_HPP_INCLUDED

#include <cstddef> // std::size_t 

namespace chops {
namespace net {

/**
 *  @brief @c output_queue_stats provides information on the internal output 
 *  queue.
 */

struct output_queue_stats {

  std::size_t output_queue_size;
  std::size_t bytes_in_output_queue;
  // std::size_t total_bufs_sent;
  // std::size_t total_bytes_sent;
};

} // end net namespace
} // end chops namespace

#endif

