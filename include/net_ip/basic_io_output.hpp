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

#include <memory> // std::weak_ptr, std::shared_ptr
#include <system_error>
#include <cstddef> // std::size_t, std::byte
#include <utility> // std::move

#include "nonstd/expected.hpp"

#include "marshall/shared_buffer.hpp"
#include "net_ip/queue_stats.hpp"

#include "net_ip/detail/wp_access.hpp"

namespace chops {
namespace net {


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

public:
  using endpoint_type = typename IOT::endpoint_type;

public:

/**
 *  @brief Default construct a @c basic_io_output.
 *
 *  A @c basic_io_output is not useful until an active @c basic_io_output is assigned
 *  into it.
 *  
 */
  basic_io_output() = default;

/**
 *  @brief Construct a @c std::weak_ptr to an internal IO handler. This constructor is for 
 *  internal use only and not to be used by application code.
 */
  explicit basic_io_output(std::weak_ptr<IOT> p) noexcept : m_ioh_wptr(p) { }

/**
 *  @brief Query whether an IO handler is associated with this object.
 *
 *  If @c true, an IO handler (e.g. TCP or UDP IO handler) is associated. However, 
 *  the IO handler may be closed or shutting down, which means it will not queue
 *  any sent data.
 *
 *  @return @c true if associated with an IO handler.
 */
  bool is_valid() const noexcept { return !m_ioh_wptr.expired(); }

/**
 *  @brief Return output queue statistics, allowing application monitoring of output queue
 *  sizes.
 *
 *  @return @c nonstd::expected - @c output_queue_stats on success; on error (if no
 *  associated IO handler), a @c std::error_code is returned.
 *
 */
  auto get_output_queue_stats() const ->
         nonstd::expected<output_queue_stats, std::error_code> {
    return detail::wp_access<output_queue_stats>( m_ioh_wptr,
          [] (std::shared_ptr<IOT> sp) { return sp->get_output_queue_stats(); } );
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
 *  @return @c true if buffer queued for output, @c false otherwise (no IO handler
 *  association, or .
 *
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
 */
  bool send(chops::mutable_shared_buffer&& buf, const endpoint_type& endp) const {
    return send(chops::const_shared_buffer(std::move(buf)), endp);
  }

/**
 *  @brief Compare two @c basic_io_output objects for equality.
 *
 *  The comparison is made through the @c std::shared_ptr @c operator== method. The 
 *  comparison is made on addresses if both @c basic_io_output objects are valid. If 
 *  both @c basic_io_output objects are invalid, @c true is returned (this 
 *  implies that all invalid @c basic_io_output objects are equivalent). If one is valid 
 *  and the other invalid, @c false is returned.
 *
 *  @return As described in the comments.
 */
  bool operator==(const basic_io_output<IOT>& rhs) const noexcept {
    return (m_ioh_wptr.lock() == rhs.m_ioh_wptr.lock());
  }

/**
 *  @brief Compare two @c basic_io_output objects for ordering purposes.
 *
 *  The comparison is made through the @c std::shared_ptr @c operator< method. 
 *  All invalid @c basic_io_output objects are less than valid ones. When both are valid,
 *  the address ordering is returned.
 *
 *  @return As described in the comments.
 */
  bool operator<(const basic_io_output<IOT>& rhs) const noexcept {
    return (m_ioh_wptr.lock() < rhs.m_ioh_wptr.lock());
  }

};

} // end net namespace
} // end chops namespace

#endif

