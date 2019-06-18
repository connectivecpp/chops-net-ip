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

#include "nonstd/expected.hpp"

#include "marshall/shared_buffer.hpp"
#include "net_ip/queue_stats.hpp"

#include "net_ip/net_ip_error.hpp"
#include "net_ip/detail/wp_access.hpp"

namespace chops {
namespace net {

namespace detail {

}


/**
 *  @brief The @c basic_io_output class template provides methods for sending data to 
 *  an associated network IO handler (TCP or UDP IO handler) or getting output queue
 *  statistics.
 *
 *  The @c basic_io_output class provides the primary application interface for
 *  network IO data sending, whether TCP or UDP. This class provides methods to 
 *  send data and query output queue stats.
 *
 *  Unless default constructed, a @c basic_io_output object has an association 
 *  to an IO handler object. 
 *
 *  This class is a lightweight value class, allowing @c basic_io_output 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network IO handler.
 *
 *  All @c basic_io_output @c send methods can be called concurrently from multiple threads.
 *
 */

template <typename IOT>
class basic_io_output {

private:
  std::weak_ptr<IOT>   m_ioh_wptr;
  IOT*                 m_ioh_raw_ptr;

public:
  using endpoint_type = typename IOT::endpoint_type;

public:

  basic_io_output() noexcept : m_ioh_wptr(), m_ioh_raw_ptr(nullptr) { }

/**
 *  @brief Construct with a pointer to an internal IO handler, where the @c std::weak_ptr
 *  association is not needed, primarily used within the lifetime of a message handler
 *  callback. This constructor is for internal use only and not to be used
 *  by application code.
 */
  explicit basic_io_output(IOT* ioh) noexcept : m_ioh_wptr(), m_ioh_raw_ptr(ioh) { }

/**
 *  @brief Construct a @c std::weak_ptr to an internal IO handler. This constructor is for 
 *  internal use only and not to be used by application code.
 */
  explicit basic_io_output(std::weak_ptr<IOT> p) noexcept : m_ioh_wptr(p), 
                                                            m_ioh_raw_ptr(nullptr) { }


/**
 *  @brief Query whether an IO handler is associated with this object.
 *
 *  If @c true, an IO handler (e.g. TCP or UDP IO handler) is associated. However, 
 *  the IO handler may be closed or shutting down, which means it will not queue
 *  any sent data.
 *
 *  @return @c true if associated with an IO handler.
 */
  bool is_valid() const noexcept { return m_ioh_raw_ptr == nullptr ? !m_ioh_wptr.expired() : true; }

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
    return m_ioh_ptr->get_output_queue_stats();
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
    return m_ioh_ptr->send(buf);
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
    return m_ioh_ptr->send(buf, endp);
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

/**
 *  @brief Compare two @c basic_io_output objects for equality.
 *
 *  @return @c true if both @c basic_io_output object point to the same internal
 *  IO handler.
 */
  bool operator==(const basic_io_output<IOT>& rhs) const noexcept {
    return (m_ioh_ptr == rhs.m_ioh_ptr);
  }

/**
 *  @brief Compare two @c basic_io_output objects for ordering purposes.
 *
 *  @return Comparison made through IO handler pointers, no association
 *  will compare less than one with an association.
 */
  bool operator<(const basic_io_output<IOT>& rhs) const noexcept {
    return (m_ioh_ptr < rhs.m_ioh_ptr);
  }

};

} // end net namespace
} // end chops namespace

#endif

