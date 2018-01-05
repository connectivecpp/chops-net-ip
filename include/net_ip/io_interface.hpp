/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c io_interface class template and related functionality.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_INTERFACE_HPP_INCLUDED
#define IO_INTERFACE_HPP_INCLUDED

#include <memory> // std::weak_ptr, std::shared_ptr
#include <string_view>
#include <system_error>
#include <cstddef> // std::size_t

#include "utility/shared_buffer.hpp"

#include "net_ip/net_ip_error.hpp"
#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c io_interface class provides access to an underlying network
 *  IO handler (TCP or UDP IO handler).
 *
 *  The @c io_interface class provides the primary application interface for
 *  network IO processing, whether TCP or UDP. This class provides methods to start IO
 *  processing (i.e. start read processing and enable write processing),
 *  stop IO processing, send data, query output queue stats, and access the IO handler
 *  socket.
 *
 *  This class is a lightweight value class, allowing @c io_interface 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network IO handler. Internally, a @c std::weak pointer is used 
 *  to link the @c io_interface object with a network IO handler.
 *
 *  An @c io_interface object is provided for application use through a state change 
 *  function object callback. This occurs when a @c net_entity creates the underlying 
 *  network IO handler, or the network IO handler is being closed and destucted. 
 *  The @c net_entity class documentation provides more detail.
 *
 *  An @c io_interface object is either associated with a network IO handler
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present. Note that even if the @c std::weak_pointer is 
 *  valid, the network IO handler might be in the process of closing or being 
 *  destructed. Calling the @c send method while the network IO handler is being 
 *  closed or destructed may result in loss of data.
 *
 *  Applications can default construct an @c io_interface object, but it is not useful 
 *  until a valid @c io_interface object is assigned to it (typically this would be 
 *  performed in the state change function object callback).
 *
 *  The network IO handler socket can be accessed through this interface. This allows 
 *  socket options to be queried and set (or other useful socket methods to be called).
 *
 *  Appropriate comparison operators are provided to allow @c io_interface objects
 *  to be used in associative or sequence containers.
 *
 *  All @c io_interface methods can be called concurrently from multiple threads.
 *  In particular, @c send methods are allowed to be called concurrently. Calling 
 *  other methods concurrently (such as @c stop_io or @c start_io) will not 
 *  result in a crash, but could result in undesired application behavior.
 *
 */

template <typename IOH>
class io_interface {
private:
  std::weak_ptr<IOH> m_ioh_wptr;

public:
  using endpoint_type = typename IOH::endpoint_type;

public:

/**
 *  @brief Default construct an @c io_interface.
 *
 *  An @c io_interface is not useful until an active IO handler object is assigned
 *  into it.
 *  
 */
  io_interface() = default;

  io_interface(const io_interface&) = default;
  io_interface(io_interface&&) = default;

  io_interface<IOH>& operator=(const io_interface&) = default;
  io_interface<IOH>& operator=(io_interface&&) = default;
  
/**
 *  @brief Construct with a shared weak pointer to an internal IO handler, used
 *  internally and not by application code.
 *
 */
  explicit io_interface(std::weak_ptr<IOH> p) noexcept : m_ioh_wptr(p) { }

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
 *  @brief Query whether @c start_io on an IO handler has been called or not.
 *
 *  @return @c true if @c start_io has been called, @c false otherwise.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated IO handler.
 */
  bool is_started() const {
    if (auto p = m_ioh_wptr.lock()) {
      return p->is_started();
    }
    throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
  }

/**
 *  @brief Return a reference to the underlying socket, allowing socket options to be queried 
 *  or set or other socket methods to be called.
 *
 *  @return @c ip::tcp::socket or @c ip::udp::socket, depending on IO handler type.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated IO handler.
 */
  typename IOH::socket_type& get_socket() const {
    if (auto p = m_ioh_wptr.lock()) {
      return p->get_socket();
    }
    throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
  }

/**
 *  @brief Return output queue statistics, allowing application monitoring of output queue
 *  sizes.
 *
 *  @return @c queue_status if network IO handler is available.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated IO handler.
 */
  output_queue_stats get_output_queue_stats() const {
    if (auto p = m_ioh_wptr.lock()) {
      return p->get_output_queue_stats();
    }
    throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
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
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool send(const void* buf, std::size_t sz) const { return send(chops::const_shared_buffer(buf, sz)); }

/**
 *  @brief Send a reference counted buffer through the associated network IO handler.
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::const_shared_buffer containing data.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool send(chops::const_shared_buffer buf) const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->send(buf), true) : false;
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
 *    an_io_interface.send(std::move(buf));
 *    buf.resize(0); // or whatever new desired size
 *  @endcode
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c chops::mutable_shared_buffer containing data.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool send(chops::mutable_shared_buffer&& buf) const { return send(chops::const_shared_buffer(buf)); }

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
 *  @param endp Destination @c std::experimental::net::ip::udp::endpoint.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
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
 *  @param endp Destination @c std::experimental::net::ip::udp::endpoint.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool send(chops::const_shared_buffer buf, const endpoint_type& endp) const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->send(buf, endp), true) : false;
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
 *  @param endp Destination @c std::experimental::net::ip::udp::endpoint.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool send(chops::mutable_shared_buffer&& buf, const endpoint_type& endp) const {
    return send(chops::const_shared_buffer(buf), endp);
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
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *  @code
 *    bool (std::experimental::net::const_buffer,
 *          chops::net::io_interface<chops::net::tcp_io_type>,
 *          std::experimental::net::ip::tcp::endpoint);
 *  @endcode
 *  The buffer always references from the beginning of the full message. The 
 *  @c io_interface can be used for sending a reply. The endpoint is the remote 
 *  endpoint that sent the data. 

 *  Returning @c false from the message handler callback causes the connection to be 
 *  closed.
 *
 *  @param msg_frame A message frame function object callback. The signature of
 *  the callback is:
 *  @code
 *    std::size_t (std::experimental::net::mutable_buffer);
 *  @endcode
 *  The complete incoming buffer is passed in, no matter how many times the
 *  message frame object has been called in assembling the complete message. The 
 *  callback returns the size of the next read, or zero as a notification that the 
 *  complete message has been called and the message handler is to be invoked.
 *
 *  If there is non-trivial processing that is performed in the message frame
 *  object and the application wishes to keep any resulting state (typically to
 *  use within the message handler), two options (at least) are available. 
 *  1) Store a reference to the message frame object from within the message handler 
 *  object, or 2) Design a single class that provides two function call overloads and
 *  use the same object for both the message handler and the message frame processing.
 *
 *  @param header_size The initial read size (in bytes) of each incoming message.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  template <typename MH, typename MF>
  bool start_io(MH && msg_handler, MF&& msg_frame, std::size_t header_size) const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->start_io(msg_handler, msg_frame, header_size), true) : false;
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
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *  @code
 *    bool (std::experimental::net::const_buffer,
 *          chops::net::io_interface<chops::net::tcp_io_type>,
 *          std::experimental::net::ip::tcp::endpoint);
 *  @endcode
 *  The buffer points to the complete message including the delimiter sequence. The 
 *  @c io_interface can be used for sending a reply, and the endpoint is the remote 
 *  endpoint that sent the data. Returning @c false from the message handler callback 
 *  causes the connection to be closed.
 *
 *  @param delimiter Delimiter characters denoting end of each message.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  template <typename MH>
  bool start_io(MH&& msg_handler, std::string_view delimiter) const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->start_io(msg_handler, delimiter), true) : false;
  }

/**
 *  @brief Enable IO processing for the associated network IO handler with fixed
 *  or maximum buffer size logic.
 *
 *  This method is implemented for both TCP and UDP IO handlers.
 *
 *  For TCP IO handlers, this reads fixed size messages.
 *
 *  For UDP IO handlers, this specifies the maximum size of the datagram. For IPv4 this
 *  value can be up to 65,507, and larger for IPv6. If the incoming datagram contains
 *  a size larger than the specified size, data will be truncated (lost).
 *
 *  @param msg_handler A message handler function object callback. The signature of
 *  the callback is:
 *  @code
 *    bool (std::experimental::net::const_buffer,
 *          chops::net::io_interface<IO_type>,
 *          Endpoint_type)
 *  @endcode
 *  For UDP the buffer size denotes the incoming datagram size. For TCP the buffer size will 
 *  always match the "read_size" parameter. The "IO_type" is either @c chops::net::tcp_io_type or
 *  @c chops::net::udp_io_type. The "Endpoint_type" is either 
 *  @c std::experimental::net::ip::udp::endpoint or @c std::experimental::net::ip::tcp::endpoint.
 *  Returning @c false from the message handler callback causes the TCP connection or UDP socket to 
 *  be closed.
 *
 *  @param read_size Maximum UDP datagram size or fixed TCP read size.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */

  template <typename MH>
  bool start_io(MH&& msg_handler, std::size_t read_size) const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->start_io(msg_handler, read_size), true) : false;
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
 *  this read completes it is due to an error condition). For UDP IO handlers, no
 *  reads are started.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */

  bool start_io() {
    auto p = m_ioh_wptr.lock();
    return p ? (p->start_io(), true) : false;
  }

 
/**
 *  @brief Stop IO processing and close the associated network IO handler.
 *
 *  After this call, connection or socket close processing will occur, a state change function 
 *  object callback will be invoked, and eventually the IO handler object will be destructed. 
 *  @c start_io cannot be called after @c stop_io (in other words, @c start_io followed by 
 *  @c stop_io followed by @c start_io, etc is not supported).
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  bool stop_io() const {
    auto p = m_ioh_wptr.lock();
    return p ? (p->stop_io(), true) : false;
  }


/**
 *  @brief Compare two @c io_interface objects for equality.
 *
 *  If both @io_interface objects are valid, then a @c std::shared_ptr comparison is made.
 *  If both @io_interface objects are invalid, @c true is returned (this implies that
 *  all invalid @c io_interface objects are equivalent). If one is valid and the other invalid,
 *  @c false is returned.
 *
 *  @return As described in the comments.
 */

  bool operator==(const io_interface<IOH>& rhs) const noexcept {
    auto lp = m_ioh_wptr.lock();
    auto rp = rhs.m_ioh_wptr.lock();
    return (lp && rp && lp == rp) || (!lp && !rp);
  }

/**
 *  @brief Compare two @c io_interface objects for ordering purposes.
 *
 *  All invalid @c io_interface objects are less than valid ones. When both are valid,
 *  the @c std::shared_ptr ordering is returned.
 *
 *  Specifically, if both @c io_interface objects are invalid @c false is returned. 
 *  If the left @c io_interface object is invalid and the right is not, @c true is
 *  returned. If the right @c io_interface is invalid, but the left is not, @c false is 
 *  returned.
 *
 *  @return As described in the comments.
 */
  bool operator<(const io_interface<IOH>& rhs) const noexcept {
    auto lp = m_ioh_wptr.lock();
    auto rp = rhs.m_ioh_wptr.lock();
    return (lp && rp && lp < rp) || (!lp && rp);
  }

};

} // end net namespace
} // end chops namespace

#endif

