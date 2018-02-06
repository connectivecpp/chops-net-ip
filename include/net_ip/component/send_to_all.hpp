/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief A class template that manages a collection of 
 *  @c io_interface objects and provides "send to all" functionality.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SEND_TO_ALL_HPP_INCLUDED
#define SEND_TO_ALL_HPP_INCLUDED

#include <cstddef> // std::size_t
#include <utility> // std::move

#include <mutex>
#include <vector>

#include "net_ip/io_interface.hpp"

#include "utility/erase_where.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {

/**
 *  @brief Manage a collection of @c io_interface objects and provide a way
 *  to send data to all.
 *
 *  In addition to providing "send to all" functionality, this class overloads
 *  the function call operator so that it can be used as a "stop function" in
 *  the @c net_entity @c start method. If used in this manner, the functional
 *  operator deletes the @c io_interface object from the collection.
 *
 */
template <typename IOH>
class send_to_all {
private:
  using lock_guard = std::lock_guard<std::mutex>;
  using io_intfs   = std::vector<io_interface<IOH> >;

private:
  mutable std::mutex    m_mutex;
  io_intfs              m_io_intfs;

public:
  void add_io_interface(io_interface<IOH> io) {
    lock_guard gd { m_mutex };
    m_io_intfs.push_back(io);
  }

  void remove_io_interface(io_interface<IOH> io) {
    lock_guard gd { m_mutex };
    chops::erase_where(m_io_intfs, io);
  }

  void operator() (io_interface<IOH> io, std::error_code /* err */, std::size_t /* num */ ) {
    if (io.is_valid()) {
      remove_io_interface(io);
    }
  }

  void send(chops::const_shared_buffer buf) const {
    lock_guard gd { m_mutex };
    for (auto io : m_io_intfs) {
      io.send(buf);
    }
  }
  void send(const void* buf, std::size_t sz) const {
    send(chops::const_shared_buffer(buf, sz));
  }
  void send(chops::mutable_shared_buffer&& buf) const { 
    send(chops::const_shared_buffer(std::move(buf)));
  }

  std::size_t size() const noexcept {
    lock_guard gd { m_mutex };
    return m_io_intfs.size();
  }

  std::size_t total_output_queue_size() const noexcept {
    std::size_t tot = 0;
    lock_guard gd { m_mutex };
    for (auto io : m_io_intfs) {
      tot += io.get_output_queue_stats().output_queue_size;
    }
    return tot;
  }
};

} // end net namespace
} // end chops namespace

#endif

