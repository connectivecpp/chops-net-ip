/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c basic_io_interface class template, providing @c start_io, @c stop_io,
 *  @c visit_socket, @c make_io_output and related methods.
 *  methods.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef BASIC_IO_INTERFACE_HPP_INCLUDED
#define BASIC_IO_INTERFACE_HPP_INCLUDED

#include <memory> // std::weak_ptr, std::shared_ptr
#include <string_view>
#include <system_error>
#include <cstddef> // std::size_t, std::byte
#include <utility> // std::forward, std::move

#include "nonstd/expected.hpp"

#include "net_ip/net_ip_error.hpp"
#include "net_ip/basic_io_output.hpp"

#include "net_ip/simple_variable_len_msg_frame.hpp"

#include "net_ip/detail/wp_access.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c basic_io_interface class template provides access to an underlying 
 *  network IO handler (TCP or UDP IO handler), primarily for calling the @c start_io,
 *  @c stop_io, and @c make_io_output methods.
 *
 *  The @c basic_io_interface class provides the primary application interface to start
 *  network IO processing, whether TCP or UDP. This class provides methods to start IO
 *  processing (i.e. start read processing and enable write processing),
 *  stop IO processing (if needed, typically doesn't need to be directly called), and 
 *  access the IO handler socket (e.g. to retrieve or modify socket options). It also 
 *  provides a method to create a @c basic_io_output object for sending data.
 *
 *  This class is a lightweight value class, allowing @c basic_io_interface 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network IO handler. Internally, a @c std::weak pointer is used 
 *  to link the @c basic_io_interface object with a network IO handler.
 *
 *  A @c basic_io_interface object is provided for application use through a state change 
 *  function object callback. This occurs when a @c net_entity creates the underlying 
 *  network IO handler, or the network IO handler is being closed and destructed. 
 *  The @c net_entity class documentation provides more detail.
 *
 *  A @c basic_io_interface object is either associated with a network IO handler
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present. Note that even if the @c std::weak_pointer is 
 *  valid, the network IO handler might be in the process of closing or being 
 *  destructed.
 *
 *  Applications can default construct a @c basic_io_interface object, but it is not useful 
 *  until a valid @c basic_io_interface object is assigned to it (typically this would be 
 *  performed through the state change function object callback).
 *
 *  A @c basic_io_output object can be created from a @c basic_io_interface, allowing 
 *  the sending of data to the underlying network IO handler.
 *
 *  The underlying network IO handler socket can be accessed through the @c visit_socket
 *  method. This allows socket options to be queried and set (or other useful socket methods 
 *  to be called).
 *
 *  Appropriate comparison operators are provided to allow @c basic_io_interface objects
 *  to be used in associative or sequence containers.
 *
 *  All @c basic_io_interface methods can be called concurrently from multiple threads, but
 *  calling @c stop_io at the same time as @c start_io from multiple threads may result
 *  in undesired application behavior.
 *
 *  Error handling is provided by returning a @c nonstd::expected object on most methods. 
 *  
 */

template <typename IOT>
class basic_io_interface {
private:
  std::weak_ptr<IOT> m_ioh_wptr;

public:
  using endpoint_type = typename IOT::endpoint_type;

public:

/**
 *  @brief Default construct a @c basic_io_interface.
 *
 *  A @c basic_io_interface is not useful until an active @c basic_io_interface is assigned
 *  into it.
 *  
 */
  basic_io_interface() = default;

  basic_io_interface(const basic_io_interface&) = default;
  basic_io_interface(basic_io_interface&&) = default;

  basic_io_interface<IOT>& operator=(const basic_io_interface&) = default;
  basic_io_interface<IOT>& operator=(basic_io_interface&&) = default;
  
/**
 *  @brief Construct with a shared weak pointer to an internal IO handler, this is an
 *  internal constructor only and not to be used by application code.
 *
 */
  explicit basic_io_interface(std::weak_ptr<IOT> p) noexcept : m_ioh_wptr(p) { }

/**
 *  @brief Query whether an IO handler is associated with this object.
 *
 *  If @c true, an IO handler (e.g. TCP or UDP IO handler) is associated. However, 
 *  the IO handler may be closing down and may not allow further operations.
 *
 *  @return @c true if associated with an IO handler.
 */
  bool is_valid() const noexcept { return !m_ioh_wptr.expired(); }

/**
 *  @brief Make a @c basic_io_output object from the associated IO handler.
 *
 *  A @c basic_io_output object is used for sending data. A @c basic_io_output object
 *  will be created even if @c start_io has not been called, and attempting to send
 *  data before @c start_io is called will result in discarded data.
 *
 *  @return @c nonstd::expected - @c basic_io_output on success, either a @c tcp_io_output 
 *  or @c udp_io_output; on error (if no associated IO handler), a @c std::error_code is 
 *  returned.
 *
 */
  auto make_io_output() const ->
        nonstd::expected<basic_io_output<IOT>, std::error_code> {
    return detail::wp_access<basic_io_output<IOT>>( m_ioh_wptr, 
          [] (std::shared_ptr<IOT> sp) { return basic_io_output<IOT>(sp); } );
  }

/**
 *  @brief Query whether an IO handler is in a started state or not.
 *
 *  @return @c nonstd::expected - @c bool on success, specifying whether 
 *  @c start_io on the IO handler has been called (if @c false, the IO handler has not 
 *  been started or is in a stopped state); on error (if no associated IO handler), a 
 *  @c std::error_code is returned.
 *
 */
  auto is_io_started() const ->
        nonstd::expected<bool, std::error_code> {
    return detail::wp_access<bool>( m_ioh_wptr, 
          [] (std::shared_ptr<IOT> sp) { return sp->is_io_started(); } );
  }

/**
 *  @brief Provide an application supplied function object which will be called with a 
 *  reference to the associated IO handler socket.
 *
 *  The function object must have one of the following signatures, depending on the IO handler
 *  type:
 *
 *  @code
 *    void (asio::ip::tcp::socket&); // TCP IO
 *    void (asio::ip::udp::socket&); // UDP IO
 *  @endcode
 *
 *  Within the function object socket options can be queried or modified or any valid method
 *  called.
 *
 *  @return @c nonstd::expected - socket has been visited on success; on error (if no 
 *  associated IO handler), a @c std::error_code is returned.
 */
  template <typename F>
  auto visit_socket(F&& func) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr, [&func] (std::shared_ptr<IOT> sp) {
            sp->visit_socket(func); return std::error_code { }; } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with message 
 *  frame logic.
 *
 *  This method is not implemented for UDP IO handlers.
 *
 *  For TCP IO handlers, this starts read processing using a message handler 
 *  function object callback and a message frame function object callback. Sends 
 *  (writes) are enabled after this call.
 *
 *  This method is used for message frame based TCP processing. The logic is
 *  "read header, process data to determine size of rest of message, read rest
 *  of message". The message frame function object callback implements this
 *  logic. Once a complete message has been read, the message handler function 
 *  object callback is invoked. There may be multiple iterations of calling
 *  the message frame function object before the message frame determines the
 *  complete message has been read.
 *
 *  @param header_size The initial read size (in bytes) of each incoming message.
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    bool (asio::const_buffer,
 *          chops::net::tcp_io_output, // basic_io_output<tcp_io>
 *          asio::ip::tcp::endpoint);
 *  @endcode
 *
 *  The buffer (first parameter) always references a full message. The 
 *  @c basic_io_output can be used for sending a reply. The endpoint is the remote 
 *  endpoint that sent the data (not used in the @c send method call, but may be
 *  useful for other purposes). 
 *
 *  Returning @c false from the message handler callback causes the connection to be 
 *  closed.
 *
 *  The message handler function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @param msg_frame A message frame function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    std::size_t (asio::mutable_buffer);
 *  @endcode
 *
 *  Each time the message frame callback is called by the Chops Net IP IO handler, the 
 *  next chunk of incoming bytes is passed through the buffer parameter.
 *
 *  The callback returns the size of the next read, or zero as a notification that the 
 *  complete message has been called and the message handler is to be invoked.
 *
 *  If there is non-trivial processing that is performed in the message frame
 *  object and the application wishes to keep any resulting state (typically to
 *  use within the message handler), one option is to design a single class that provides 
 *  two operator function call overloads and use the same object (via @c std::ref) for both 
 *  the message handler and the message frame processing.
 *
 *  The message frame function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 *
 */
  template <typename MH, typename MF>
  auto start_io(std::size_t header_size, MH&& msg_handler, MF&& msg_frame) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [header_size, &msg_handler, &msg_frame] (std::shared_ptr<IOT> sp) {
            return sp->start_io(header_size, msg_handler, msg_frame) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with simple
 *  variable length message frame logic.
 *
 *  This method is not implemented for UDP IO handlers.
 *
 *  For TCP IO handlers, this starts read processing using a message handler 
 *  function object callback and a simple variable length message header decoder function
 *  pointer. The message frame TCP processing is similar to the generalized message
 *  frame processing except that only simple variable length messages are supported. "Simple"
 *  means a fixed size header followed by one variable length body. The header must be
 *  able to be decoded with a simple function that returns the size (in bytes) of the body
 *  when given the full, fixed size header.
 *
 *  @param header_size The initial read size (in bytes) of each incoming message, which should
 *  match the header size of the header decoder function.
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    bool (asio::const_buffer,
 *          chops::net::tcp_io_output, // basic_io_output<tcp_io>
 *          asio::ip::tcp::endpoint);
 *  @endcode
 *
 *  The buffer (first parameter) always references a full message. The 
 *  @c basic_io_output can be used for sending a reply. The endpoint is the remote 
 *  endpoint that sent the data (not used in the @c send method call, but may be
 *  useful for other purposes). 
 *
 *  Returning @c false from the message handler callback causes the connection to be 
 *  closed.
 *
 *  The message handler function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @param func A function that is of type @c hdr_decoder_func.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  template <typename MH>
  auto start_io(std::size_t header_size, MH&& msg_handler, hdr_decoder_func func) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [header_size, &msg_handler, func] (std::shared_ptr<IOT> sp) {
            return sp->start_io(header_size, msg_handler, func) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with delimeter 
 *  logic.
 *
 *  This method is not implemented for UDP IO handlers.
 *
 *  For TCP IO handlers, this starts read processing using a message handler
 *  function object callback. Sends (writes) are enabled after this call.
 *
 *  This method is used for delimiter based TCP processing (typically text based 
 *  messages in TCP streams). The logic is "read data until the delimiter characters 
 *  match" (which are usually "end-of-line" sequences). The message handler function 
 *  object callback is then invoked.
 *
 *  @param delimiter Delimiter characters denoting end of each message.
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    bool (asio::const_buffer,
 *          chops::net::tcp_io_output, // basic_io_output<tcp_io>
 *          asio::ip::tcp::endpoint);
 *  @endcode
 *
 *  The buffer points to the complete message including the delimiter sequence. The 
 *  @c basic_io_output can be used for sending a reply, and the endpoint is the remote 
 *  endpoint that sent the data. Returning @c false from the message handler callback 
 *  causes the connection to be closed.
 *
 *  The message handler function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 *
 */
  template <typename MH>
  auto start_io(std::string_view delimiter, MH&& msg_handler) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [delimiter, &msg_handler] (std::shared_ptr<IOT> sp) {
            return sp->start_io(delimiter, msg_handler) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with fixed
 *  or maximum buffer size logic.
 *
 *  This method is implemented for both TCP and UDP IO handlers.
 *
 *  For TCP IO handlers, this reads a fixed size message which is then passed to the
 *  message handler function object.
 *
 *  For UDP IO handlers, this specifies the maximum size of the datagram. For IPv4 this
 *  value can be up to 65,507 (for IPv6 the maximum is larger). If the incoming datagram 
 *  is larger than the specified size, data will be truncated or lost.
 *
 *  Sends (writes) are enabled after this call.
 *
 *  @param read_size Maximum UDP datagram size or TCP fixed read size.
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    // TCP io:
 *    bool (asio::const_buffer,
 *          chops::net::tcp_io_output, // basic_io_output<tcp_io>
 *          asio::ip::tcp::endpoint);
 *    // UDP io:
 *    bool (asio::const_buffer,
 *          chops::net::udp_io_output, // basic_io_output<udp_io>
 *          asio::ip::udp::endpoint);
 *  @endcode
 *
 *  Returning @c false from the message handler callback causes the TCP connection or UDP socket to 
 *  be closed.
 *
 *  The message handler function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  template <typename MH>
  auto start_io(std::size_t read_size, MH&& msg_handler) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [read_size, &msg_handler] (std::shared_ptr<IOT> sp) {
            return sp->start_io(read_size, msg_handler) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with a maximum 
 *  buffer size and a default destination endpoint.
 *
 *  This method is not implemented for TCP IO handlers (only for UDP IO handlers).
 *
 *  This allows the @c send method without an endpoint to be called for UDP datagrams.
 *  The buffer will be sent to this destination endpoint.
 *
 *  The maximum size parameter is the same as the corresponding (without default 
 *  destination endpoint) @c start_io method.
 *
 *  Sends (writes) are enabled after this call.
 *
 *  @param endp Default destination @c asio::ip::udp::endpoint.
 *
 *  @param max_size Maximum UDP datagram size.
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *
 *  @code
 *    bool (asio::const_buffer,
 *          chops::net::udp_io_output, // basic_io_output<udp_io>
 *          asio::ip::udp::endpoint);
 *  @endcode
 *
 *  Returning @c false from the message handler callback causes the UDP socket to 
 *  be closed.
 *
 *  The message handler function object is moved if possible, otherwise it is copied. 
 *  State data should be movable or copyable.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  template <typename MH>
  auto start_io(const endpoint_type& endp, std::size_t max_size, MH&& msg_handler) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [endp, max_size, &msg_handler] (std::shared_ptr<IOT> sp) {
            return sp->start_io(endp, max_size, msg_handler) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with no incoming
 *  message handling.
 *
 *  This method is implemented for both TCP and UDP IO handlers.
 *
 *  This method is used to enable IO processing where only sends are needed (and no 
 *  incoming message handling).
 *
 *  For TCP IO handlers, a read will be started, but no data processed (typically if
 *  the read completes it is due to an error condition). For UDP IO handlers, no
 *  reads are started.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  auto start_io() ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [] (std::shared_ptr<IOT> sp) {
            return sp->start_io() ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with no incoming
 *  message handling, but with a default destination endpoint.
 *
 *  This method is not implemented for TCP IO handlers (only for UDP IO handlers).
 *
 *  This allows the @c send method without an endpoint to be called for UDP datagrams.
 *
 *  This method is used to enable IO processing where only sends are needed (and no 
 *  incoming message handling).
 *
 *  @param endp Default destination @c asio::ip::udp::endpoint.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  auto start_io(const endpoint_type& endp) ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [endp] (std::shared_ptr<IOT> sp) {
            return sp->start_io(endp) ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_started);
        } );
  }
 
/**
 *  @brief Stop IO processing and close the associated network IO handler.
 *
 *  After this call, connection disconnect or socket close processing will occur, a state 
 *  change function object callback will be invoked, and eventually the IO handler object 
 *  will be destructed. @c start_io cannot be called after @c stop_io (in other words, 
 *  @c start_io followed by @c stop_io followed by @c start_io, etc is not supported).
 *
 *  For TCP entities (connectors and acceptors), the TCP connection will be taken
 *  down, but the entity will remain active. For UDP entities @c stop_io is equivalent
 *  to calling @c stop on the @c net_entity.
 *
 *  @return @c nonstd::expected - IO is started on success; on error, a 
 *  @c std::error_code is returned.
 */
  auto stop_io() ->
        nonstd::expected<void, std::error_code> {
    return detail::wp_access_void( m_ioh_wptr,
                [] (std::shared_ptr<IOT> sp) {
            return sp->stop_io() ? std::error_code() :
                                std::make_error_code(net_ip_errc::io_already_stopped);
        } );
  }

/**
 *  @brief Compare two @c basic_io_interface objects for equality.
 *
 *  The comparison is made through the @c std::shared_ptr @c operator== method. The 
 *  comparison is made on addresses if both @c basic_io_interface objects are valid. If 
 *  both @c basic_io_interface objects are invalid, @c true is returned (this 
 *  implies that all invalid @c basic_io_interface objects are equivalent). If one is valid 
 *  and the other invalid, @c false is returned.
 *
 *  @return As described in the comments.
 */
  bool operator==(const basic_io_interface<IOT>& rhs) const noexcept {
    return (m_ioh_wptr.lock() == rhs.m_ioh_wptr.lock());
  }

/**
 *  @brief Compare two @c basic_io_interface objects for ordering purposes.
 *
 *  The comparison is made through the @c std::shared_ptr @c operator< method. 
 *  All invalid @c basic_io_interface objects are less than valid ones. When both are valid,
 *  the address ordering is returned.
 *
 *  @return As described in the comments.
 */
  bool operator<(const basic_io_interface<IOT>& rhs) const noexcept {
    return (m_ioh_wptr.lock() < rhs.m_ioh_wptr.lock());
  }

/**
 *  @brief Return a raw pointer to an associated IO handler.
 *
 *  This method provides a raw pointer to an associated IO handler (or @c nullptr). This
 *  value can be used for logging or associative container lookup purposes. In particular,
 *  this value can be used to correlate multiple error messages from the same IO handler
 *  instantiation.
 *
 *  @return A pointer value, which may be a @c nullptr.
 */
  const void* get_ptr() const noexcept {
    return static_cast<const void*>(m_ioh_wptr.lock().get());
  }

};

} // end net namespace
} // end chops namespace

#endif

