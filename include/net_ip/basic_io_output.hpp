/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c basic_io_output class template.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef BASIC_IO_OUTPUT_HPP_INCLUDED
#define BASIC_IO_OUTPUT_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <cstddef> // std::move

#include "utility/shared_buffer.hpp"

#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c basic_io_output class template provides methods for sending data to 
 *  an associated network IO handler (TCP or UDP IO handler).
 *
 *  The @c basic_io_output class provides the primary application interface for
 *  network IO data sending, whether TCP or UDP. This class provides methods to 
 *  send data and query output queue stats.
 *
 *  This class provides an association by reference to a network IO handler. It does
 *  not check for a valid reference, as opposed to the @c basic_io_interface class
 *  template and the @c net_entity class. It is the application responsibility to
 *  ensure that the reference is valid.
 *
 *  To ensure a valid reference, the context for a @ basic_io_output object is as follows:
 *  1) In a message handler invocation - the reference will always be valid.
 *  2) A @c basic_io_output object can be obtained from a valid @c basic_io_interface object.
 *  As long as the originating @c basic_io_interface object is valid, the @c basic_io_output
 *  object is valid.
 *
 *  This class is a lightweight value class, allowing @c basic_io_output 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network IO handler.
 *
 *  A @c basic_io_output object is created from a @c basic_io_interface object or provided
 *  by the @c Chops Net IP library, either when a connection is established (TCP connection
 *  or UDP bind) or for an incoming message.
 *
 *  All @c basic_io_output methods can be called concurrently from multiple threads.
 *  In particular, @c send methods are allowed to be called concurrently.
 *
 */

template <typename IOH>
class basic_io_output {
private:
  IOH& m_ioh;

public:
  using endpoint_type = typename IOH::endpoint_type;

public:

/**
 *  @brief Construct with a reference to an internal IO handler, this is an
 *  internal constructor only and not to be used by application code.
 *
 */
  explicit basic_io_output(IOH& ioh) noexcept : m_ioh(ioh) { }

/**
 *  @brief Return output queue statistics, allowing application monitoring of output queue
 *  sizes.
 *
 *  @return @c queue_status @c struct.
 *
 */
  output_queue_stats get_output_queue_stats() const {
    return m_ioh.get_output_queue_stats();
  }

/**
 *  @brief Send a buffer of data through the associated network IO handler.
 *
 *  The data is copied once into an internal reference counted buffer and then
 *  managed within the IO handler. This is a non-blocking call.
 *
 *  @param buf Pointer to buffer.
 *
 *  @param sz Size of buffer.
 *
 */
  void send(const void* buf, std::size_t sz) const { send(chops::const_shared_buffer(buf, sz)); }

/**
 *  @brief Send a reference counted buffer through the associated network IO handler.
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::const_shared_buffer containing data.
 *
 */
  void send(chops::const_shared_buffer buf) const {
    m_ioh.send(buf);
  }

/**
 *  @brief Move a reference counted buffer and send it through the associated network
 *  IO handler.
 *
 *  To save a buffer copy, applications can create a @c chops::mutable_shared_buffer, 
 *  fill it with data, then move it into a @c chops::const_shared_buffer. For example:
 *
 *  @code
 *    chops::mutable_shared_buffer buf;
 *    // ... fill buf with data
 *    an_io_output.send(std::move(buf));
 *    buf.resize(0); // or whatever new desired size
 *  @endcode
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::mutable_shared_buffer containing data.
 *
 */
  void send(chops::mutable_shared_buffer&& buf) const { 
    send(chops::const_shared_buffer(std::move(buf)));
  }

/**
 *  @brief Send a buffer to a specific destination endpoint (address and port), implemented
 *  only for UDP IO handlers.
 *
 *  Buffer data will be copied into an internal reference counted buffer. Calling this method
 *  is invalid for TCP IO handlers.
 *
 *  This is a non-blocking call.
 *
 *  @param buf Pointer to buffer.
 *
 *  @param sz Size of buffer.
 *
 *  @param endp Destination @c asio::ip::udp::endpoint for the buffer.
 *
 */
  void send(const void* buf, std::size_t sz, const endpoint_type& endp) const {
    send(chops::const_shared_buffer(buf, sz), endp);
  }

/**
 *  @brief Send a reference counted buffer to a specific destination endpoint (address and 
 *  port), implemented only for UDP IO handlers.
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::const_shared_buffer containing data.
 *
 *  @param endp Destination @c asio::ip::udp::endpoint for the buffer.
 *
 */
  void send(chops::const_shared_buffer buf, const endpoint_type& endp) const {
    m_ioh.send(buf, endp);
  }

/**
 *  @brief Move a reference counted buffer and send it through the associated network
 *  IO handler, implemented only for UDP IO handlers.
 *
 *  See documentation for @c send without endpoint that moves a @c chops::mutable_shared_buffer.
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::mutable_shared_buffer containing data.
 *
 *  @param endp Destination @c asio::ip::udp::endpoint for the buffer.
 *
 */
  void send(chops::mutable_shared_buffer&& buf, const endpoint_type& endp) const {
    send(chops::const_shared_buffer(std::move(buf)), endp);
  }

};

} // end net namespace
} // end chops namespace

#endif

