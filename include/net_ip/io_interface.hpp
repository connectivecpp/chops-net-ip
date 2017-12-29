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

#include "utility/shared_buffer.hpp"

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
  using endpoint_type = std::experimental::net::ip::basic_endpoint<typename IOH::protocol_type>;

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

  io_interface& (const io_interface&) = default;
  io_interface& (io_interface&&) = default;
  
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
    throw net_ip_exception(weak_ptr_expired);
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
    throw net_ip_exception(weak_ptr_expired);
  }

/**
 *  @brief Return output queue statistics, allowing application monitoring of output queue
 *  sizes.
 *
 *  @return @c queue_status if network IO handler is available.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated IO handler.
 */
  queue_stats get_output_queue_stats() const {
    if (auto p = m_ioh_wptr.lock()) {
      return p->get_output_queue_stats();
    }
    throw net_ip_exception(weak_ptr_expired);
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
 *  To save a buffer copy, applications can create a @c chops::mutable_shared_buffer, 
 *  fill it in with data, then move the buffer instead of copying. For example:
 *
 *  @code
 *    chops::mutable_shared_buffer buf;
 *    // ... fill buf with data
 *    my_io_interface.send(std::move(buf));
 *    buf.resize(0); // or whatever desired size
 *  @endcode
 *
 *  This is a non-blocking call.
 *
 *  @param buf @c const_shared_buffer containing data.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  void send(chops::const_shared_buffer buf) const {
    if (auto p = m_ioh_wptr.lock()) {
      p->send(buf);
      return true;
    }
    return false;
  }

/**
 *  @brief Send a buffer to a specific destination endpoint (address and port), implemented
 *  only in UDP IO handlers.
 *
 *  Buffer data will be copied into an internal reference counted buffer. This method call is 
 *  invalid for TCP IO handlers.
 *
 *  @param buf Pointer to buffer.
 *
 *  @param sz Size of buffer.
 *
 *  @param endp Destination @c std::experimental::net::ip::udp::endpoint.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  void send(const void* buf, std::size_t sz, const endpoint_type& endp) const {
    return send(chops::const_shared_buffer(buf, sz), endp);
  }

/**
 *  @brief Send a reference counted buffer to a specific destination endpoint (address and 
 *  port), implemented only in UDP IO handlers.
 *
 *  A @c mutable_shared_buffer can be moved (see other @c send call for details).
 * 
 *  @param buf @c const_shared_buffer containing data.
 *
 *  @return @c true if IO handler association is valid, otherwise @c false.
 */
  void send(chops::const_shared_buffer buf, const endpoint_type& endp) const {
    if (auto p = m_ioh_wptr.lock()) {
      p->send(buf, endp);
      return true;
    }
    return false;
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
  void stop_io() const {
    if (auto p = m_ioh_wptr.lock()) {
      p->stop_io();
      return true;
    }
    return false;
  }

  friend bool operator==(const io_interface<IOH>&, const io_interface<IOH>&);
  friend bool operator<(const io_interface<IOH>&, const io_interface<IOH>&);

};

/**
 *  @brief Compare two @c io_interface objects for equality.
 *
 *  If both @io_interface objects are valid, then a comparison is made to the IO handler
 *  pointers. If both @io_interface objects are invalid, @c true is returned (this implies that
 *  all invalid @c io_interface objects are equivalent). If one is valid and the other invalid,
 *  @c false is returned.
 *
 *  @return @c true if both @c io_interface objects are pointing to the same IO handler, or
 *  neither is pointing to an IO handler.
 *
 *  @relates io_interface
 */

inline bool operator==(const io_interface<IOH>& lhs, const io_interface<IOH>& rhs) noexcept {
  auto lp = lhs.m_ioh_wptr.lock();
  auto rp = rhs.m_ioh_wptr.lock();
  return (lp && rp && lp == rp) || (!lp && !rp);
}

/**
 *  @brief Compare two @c io_interface objects for ordering purposes.
 *
 *  If both @c io_interface objects are invalid @c false is returned. If both are valid,
 *  the IO handler pointer ordering is returned.
 *
 *  If the left @c io_interface object is invalid and the right is not, @c true is
 *  returned. If the right @c io_interface is invalid, but the left is not, @c false is 
 *  returned, implying that all invalid @c io_interface objects are less than valid ones.
 *
 *  @return @c true or @false as described.
 *
 *  @relates io_interface
 */
inline bool operator<(const io_interface<IOH>& lhs, const io_interface<IOH>& rhs) noexcept {
  auto lp = lhs.m_ioh_wptr.lock();
  auto rp = rhs.m_ioh_wptr.lock();
  return (lp && rp && lp < rp) || (!lp && rp);
}


} // end net namespace
} // end chops namespace

#endif

