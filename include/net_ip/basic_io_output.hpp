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
#include <memory> // std::shared_ptr

#include "marshall/shared_buffer.hpp"
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
 *  Unless default constructed or released, a @c basic_io_output object has an association 
 *  to an IO handler object. This association will keep the IO handler object in memory,
 *  even after the TCP connection or UDP socket has been closed. When output is done,
 *  either by application notification or because 
 *
 *  This class provides an association to a network IO handler. It may participate in
 *  the lifetime of the network IO handler, depending on how an object of this type is 
 *  created. It does not check for a valid reference, as opposed to the 
 *  @c basic_io_interface class template and the @c net_entity class.
 *
 *  To , the context for a @ basic_io_output object is as follows:
 *  1) In a message handler invocation - the reference will always be valid.
 *  2) A @c basic_io_output object can be obtained from a valid @c basic_io_interface object.
 *  As long as the originating @c basic_io_interface object is valid, the @c basic_io_output
 *  object is valid.
 *
 *  This class is a lightweight value class, allowing @c basic_io_output 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network IO handler.
 *
 *  All @c basic_io_output @c send methods can be called concurrently from multiple threads.
 *
 */

template <typename IOH>
class basic_io_output {

private:
  using iohsp = std::shared_ptr<IOH>;
private:
  // order of declaration is important, iohptr is obtained from iohsp
  iohsp    m_iohsp;
  IOH*     m_iohptr;

public:
  using endpoint_type = typename IOH::endpoint_type;

public:

  basic_io_output() noexcept : m_iohsp(), m_iohptr(nullptr) { }

/**
 *  @brief Construct with a pointer to an internal IO handler, where the @c shared_ptr
 *  lifetime is not needed. This constructor is for internal use only and not to be used
 *  by application code.
 */
  explicit basic_io_output(IOH* ioh) noexcept : m_iohsp(), m_iohptr(ioh) { }

/**
 *  @brief Construct with a @c std::shared_ptr to an internal IO handler, allowing IO 
 *  handler lifetime to be controlled. This constructor is for internal use only and not to 
 *  be used by application code.
 */
  explicit basic_io_output(iohsp sp) noexcept : m_iohsp(sp), m_iohptr(m_iohsp.get()) { }

/**
 *  @brief Query whether an IO handler is associated with this object.
 *
 *  If @c true, an IO handler (e.g. TCP or UDP IO handler) is associated. However, 
 *  the IO handler may be closed or shutting down, which means it will not queue
 *  any sent data.
 *
 *  @return @c true if associated with an IO handler.
 */
  bool is_valid() const noexcept { return m_iohptr != nullptr; }

/**
 *  @brief Query whether an IO handler is in a started state or not.
 *
 *  @return @c true if @c start_io has been called, @c false if the IO handler
 *  has not been started or is in a stopped state.
 *
 */
  bool is_io_started() const {
    m_iohptr->is_io_started();
  }

/**
 *  @brief Release the internal IO handler association, if present, so that 
 *  the IO handler object memory can be released (as needed).
 *
 *  Calling @c send after @c release without assigning a new (valid) @c basic_io_output
 *  object will result in dereferencing a null pointer.
 */
  void release() noexcept {
    m_iohsp.reset();
    m_iohptr = nullptr;
  }

/**
 *  @brief Return output queue statistics, allowing application monitoring of output queue
 *  sizes.
 *
 *  @return @c queue_status @c struct.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  output_queue_stats get_output_queue_stats() const {
    return m_iohptr->get_output_queue_stats();
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
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(const void* buf, std::size_t sz) const { return send(chops::const_shared_buffer(buf, sz)); }

/**
 *  @brief Send a reference counted buffer through the associated network IO handler.
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::const_shared_buffer containing data.
 *
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(chops::const_shared_buffer buf) const {
    return m_iohptr->send(buf);
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
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(chops::mutable_shared_buffer&& buf) const { 
    return send(chops::const_shared_buffer(std::move(buf)));
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
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(const void* buf, std::size_t sz, const endpoint_type& endp) const {
    return send(chops::const_shared_buffer(buf, sz), endp);
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
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(chops::const_shared_buffer buf, const endpoint_type& endp) const {
    return m_iohptr->send(buf, endp);
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
 *  @return @c true if buffer queued for output, @c false otherwise.
 *
 *  @pre The @c basic_io_output object must be associated with an IO handler (@c is_valid
 *  equals @c true).
 */
  bool send(chops::mutable_shared_buffer&& buf, const endpoint_type& endp) const {
    return send(chops::const_shared_buffer(std::move(buf)), endp);
  }

};

} // end net namespace
} // end chops namespace

#endif

