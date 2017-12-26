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

#include <memory> // std::weak_ptr

#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c io_interface class provides access to an underlying network
 *  IO handler (TCP or UDP IO handler).
 *
 *  The @c io_interface class provides the primary application interface for
 *  network IO, whether TCP or UDP. This class provides methods to start IO
 *  processing, which means to start read processing and enable write processing,
 *  stop IO processing, and to send data.
 *
 *  This class is a lightweight, value-based class, allowing @c io_interface 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network resource. Internally, a @c std::weak pointer is used 
 *  to link the @c io_interface object with a network IO handler.
 *
 *  An @c io_interface object is either associated with a network IO handler
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present. Note that even if the @c std::weak_pointer is 
 *  valid, the network IO handler might be in the process of closing or being 
 *  destroyed. Calling the @c send method if the network IO handler is being closed 
 *  may result in loss of data.
 *
 *  Notification of a network IO handler closing down or being destroyed is provided
 *  by callback function objects in the @c net_entity class.
 *
 *  Applications can default construct an @c io_interface object, but it is not useful 
 *  until another @c io_interface object associated to a network IO handler (which is 
 *  always created internally by the @c net_ip library) is assigned into it.
 *
 *  Appropriate comparison operators are provided to allow @c io_interface objects
 *  to be used in associative or sequence containers.
 *
 *  All @c io_interface methods can be called concurrently from multiple threads.
 *  In particular, @c send methods are allowed to be called from multiple threads.
 *  Calling other methods concurrently (such as @c stop_io or @c start_io) will not 
 *  result in a crash, but could result in undesired application behavior.
 *
 */

template <typename IOH>
class io_interface {
private:
  std::weak_ptr<IOH> m_ioh_ptr;

public:

/**
 *  @brief Default construct an @c io_interface.
 *
 *  An @c io_interface is not useful until an active io handler object is assigned
 *  into it.
 *  
 */
  io_interface() = default;

  io_interface(const io_interface&) = default;
  io_interface(io_interface&&) = default;

  io_interface& (const io_interface&) = default;
  io_interface& (io_interface&&) = default;
  
/**
 *  @brief Construct with a shared weak pointer to an internal io handler.
 *
 *  This is an internal library constructor, and is not meant to be used by application code.
 *
 */
  io_interface(std::weak_ptr<IOH> p) noexcept : m_ioh_ptr(p) { }

/**
 *  @brief Query whether an io handler is associated with this object.
 *
 *  If @c true, an io handler (e.g. TCP or UDP io handler) is associated. However, 
 *  the io handler may not be in a good state for further operations. Calling send or some other 
 *  method on an io handler that is being closed may result in undefined behavior.
 *
 *  @return @c true if associated with an io handler.
 */
  bool is_valid() const noexcept { return !m_ioh_ptr.expired(); }


/**
 *  @brief Access the underlying socket, allowing socket options to be queried or set.
 *
 *  @return @c ip::tcp::socket or @c ip::udp::socket, depending on io handler type.
 */
  typename IOH::socket_type& get_socket() noexcept { return m_
  std::experimental::net::ip

/**
 *  @brief Send a message through the associated @c io_interface object.
 *
 *  Send a message or buffer of data. The data is copied once into internal buffers 
 *  and ownership taken by @c SockLib facilities. @c send is a non-blocking call.
 *
 *  If the associated IO handler is a UDP IO handler, the UDP Sender network
 *  resource must have been constructed with a meaningful destination host 
 *  address and port, otherwise an exception is thrown.
 *
 *  @param buf Pointer to buffer.
 *
 *  @param sz Size of buffer.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is not present, or
 *  if the associated IO handler is a UDP Sender without a destination address or port.
 */
  void send(const char* buf, std::size_t sz) const { send(boost_adjunct::shared_const_buffer(buf, sz)); }

/**
 *  @brief Send a message through this @c io_interface object with a 
 *  reference-counted const buffer.
 *
 *  Send a message with a reference-counted const buffer of data. This allows
 *  @c send to proceed without needing to first copy the send
 *  buffer into a @c io_interface internal buffer. @c send is a non-blocking call.
 *
 *  See corresponding docs on @c send about UDP IO handler.
 *
 *  @param buf @c shared_const_buffer containing message. Reference counts will
 *  be updated, but no buffer copying performed.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void send(boost_adjunct::shared_const_buffer buf) const {
    if (detail::OutputChannelResourcePtr p = m_ioh_ptr.lock()) {
      p->send(buf);
      return;
    }
    throw SockLibResourceException("OutputChannel.send(buf)");
  }

/**
 *  @brief Send a UDP message to a specific destination endpoint (address and port).
 *
 *  Send a message or buffer of data to the specified endpoint. A copy
 *  is made of the data into internal buffers. The remote endpoint of a previous
 *  incoming datagram can be obtained with the @c io_interface @c remoteUdpEndpoint 
 *  method. Alternatively, a specific endpoint can be constructed using @c Boost
 *  facilities.
 *
 *  This method is applicable only for unicast or multicast UDP resources. If this method 
 *  is called on a TCP IO handler, the endpoint is ignored. @c send is 
 *  a non-blocking call.
 *
 *  @param buf Pointer to buffer.
 *
 *  @param sz Size of buffer.
 *
 *  @param endp Destination endpoint for this message.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void send(const char* buf, std::size_t sz, const boost::asio::ip::udp::endpoint& endp) const {
    send(boost_adjunct::shared_const_buffer(buf, sz), endp);
  }

/**
 *  @brief Send a UDP message to a specific endpoint, using @c shared_const_buffer.
 *
 *  @param buf @c shared_const_buffer containing message. Reference counts will
 *  be updated, but no buffer copying performed.
 *
 *  @param endp Destination endpoint for this message.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void send(boost_adjunct::shared_const_buffer buf, const boost::asio::ip::udp::endpoint& endp) const {
    if (detail::OutputChannelResourcePtr p = m_ioh_ptr.lock()) {
      p->send(buf, endp);
      return;
    }
    throw SockLibResourceException("OutputChannel.send(buf, udp endpoint)");
  }

/**
 *  @brief Stop the underlying network I/O resource.
 *
 *  Stop and close the underlying network I/O resource. This is similar to the @c stop
 *  method on an @c Embankment, but closes the network resource at a lower level.
 *  After this method is called, connection or socket close processing will occur,
 *  such as @c ChannelChangeCb callbacks being invoked.
 *
 *  The @c Embankment stop is preferred to this method, or returning @c false from
 *  an @c IncomingMsgCb. The @c io_interface @c stop method is specifically intended for 
 *  TCP Acceptor connections, where a single connection needs to be brought down outside 
 *  of incoming message callback processing. 
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void stop() const {
    if (detail::OutputChannelResourcePtr p = m_ioh_ptr.lock()) {
      p->stop();
      return;
    }
    throw SockLibResourceException("OutputChannel.stop()");
  }

/**
 *  @brief Query the remote TCP endpoint, allowing addresses and ports to be
 *  obtained as needed, such as for display purposes.
 *
 *  @return @c boost::asio::ip::tcp::endpoint, from which addresses and ports
 *  can be obtained (see Boost documentation). An empty endpoint is returned
 *  if not a TCP resource.
 *
 *  This method is safe to call concurrently (from multiple threads) at any
 *  time.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  boost::asio::ip::tcp::endpoint remoteTcpEndpoint() const {
    if (detail::OutputChannelResourcePtr p = m_ioh_ptr.lock()) {
      boost::asio::ip::tcp::endpoint endp;
      p->remoteTcpEndpoint(endp);
      return endp;
      // return p->remoteTcpEndpoint();
    }
    throw SockLibResourceException("OutputChannel.remoteTcpEndpoint");
  }

/**
 *  @brief Query the remote UDP endpoint, for datagram "return to sender" 
 *  functionality or allowing addresses and ports to be obtained as needed
 *  for display purposes.
 *
 *  @return @c boost::asio::ip::udp::endpoint, primarily for use in return
 *  datagrams. If no datagrams have been received when this method is called,
 *  an empty endpoint is returned. An empty endpoint is also returned if not a 
 *  UDP resource.
 *
 *  @note This method should only be called from within an @IncomingMsgCb callback 
 *  object. It is designed for "return a datagram to sender" functionality. If 
 *  this method is called from outside an @IncomingMsgCb at the same time as an 
 *  incoming datagram arrives, undefined results will occur.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  boost::asio::ip::udp::endpoint remoteUdpEndpoint() const {
    if (detail::OutputChannelResourcePtr p = m_ioh_ptr.lock()) {
      boost::asio::ip::udp::endpoint endp;
      p->remoteUdpEndpoint(endp);
      return endp;
      // return p->remoteUdpEndpoint();
    }
    throw SockLibResourceException("OutputChannel.remoteUdpEndpoint");
  }

  friend bool operator==(const OutputChannel&, const OutputChannel&);
  friend bool operator<(const OutputChannel&, const OutputChannel&);

};

/**
 *  @brief Compare two @c OutputChannel objects for equality.
 *
 *  Both @c OutputChannel objects must be valid (pointing to an internal network resource), 
 *  or both must be invalid (not pointing to an internal network resource), otherwise @c false 
 *  is returned.
 *
 *  This implies that all invalid @c OutputChannel objects are equivalent.
 *
 *  @return @c true if two @c OutputChannel objects are pointing to the same internal resource, or
 *  neither is pointing to an internal resource.
 *
 *  @relates OutputChannel
 */

inline bool operator==(const OutputChannel& lhs, const OutputChannel& rhs) {
  detail::OutputChannelResourcePtr plhs = lhs.m_ioh_ptr.lock();
  detail::OutputChannelResourcePtr prhs = rhs.m_ioh_ptr.lock();
  return (plhs && prhs && plhs == prhs) || (!plhs && !prhs);
}

/**
 *  @brief Compare two @c OutputChannel objects for ordering purposes, allowing @c OutputChannel objects
 *  to be stored in a @c std::map (or other associative container).
 *
 *  If both @c OutputChannel objects are invalid (not pointing to an internal network resource),
 *  this function returns @c false.
 *
 *  If the left-hand-side @c OutputChannel is invalid, but the right-hand-side is not, @c true is
 *  returned. If the right-hand-side @c OutputChannel is invalid, but the left-hand-side is not, 
 *  @c false is returned. This implies that all invalid @c OutputChannel objects are less than 
 *  valid ones.
 *
 *  Otherwise, the internal resource pointer ordering is returned.
 *
 *  @return @c true if left @c OutputChannel less than right @c OutputChannel according to internal
 *  resource pointer ordering and invalid object ordering as defined in the comments.
 *
 *  @relates OutputChannel
 */
inline bool operator<(const OutputChannel& lhs, const OutputChannel& rhs) {
  detail::OutputChannelResourcePtr plhs = lhs.m_ioh_ptr.lock();
  detail::OutputChannelResourcePtr prhs = rhs.m_ioh_ptr.lock();
  return (plhs && prhs && plhs < prhs) || (!plhs && prhs);
}


} // end net namespace
} // end chops namespace

#endif

