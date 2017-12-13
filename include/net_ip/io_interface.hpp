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

#include "Socket/SockLibException.h"
#include "Socket/OutputChannelDecls.h"
#include "Socket/detail/OutputChannelResource.h"

#include <boost_adjunct/shared_buffer.hpp> // in sandbox

#include <boost/asio/ip/udp.hpp> // udp::endpoint
#include <boost/asio/ip/tcp.hpp> // tcp::endpoint

#include <cstddef> // std::size_t
#include <string>
#include <limits>  // std::numeric_limits

namespace chops {
namespace net {

/**
 *  @brief The @c OutputChannel class provides output access through an underlying network
 *  IO resource, such as a TCP or UDP IO handler.
 *
 *  The @c OutputChannel class provides message sending facilities through an underlying
 *  network IO handler, such as a TCP or UDP IO handler. Output message queue sizes can
 *  also be managed through this class, and remote host information can be queried.
 *
 *  One additional capability is the ability to stop and close a TCP connection or a
 *  UDP port, although the specific intended use case is to close a TCP Acceptor connection 
 *  that needs to be closed outside of an incoming message callback return.
 *
 *  The @c OutputChannel class is a lightweight, value-based class, allowing @c OutputChannel 
 *  objects to be copied and used in multiple places in an application, all of them 
 *  accessing the same network resource. Internally, a "weak pointer" is used 
 *  to link the @c OutputChannel object with a network resource IO handler.
 *
 *  An @c OutputChannel object is either pointing to a valid network resource (i.e. the
 *  "weak pointer" is good), or not. The @c valid method queries this state. Note that
 *  even if the @c OutputChannel is pointing to a valid network resource, the network
 *  resource might be in the process of shutting down or being destroyed. Calling
 *  methods such as @c send when the network resource is shutting down results in 
 *  undefined behavior.
 *
 *  Notification of a network resource shutting down or being destroyed is provided through
 *  the @c ChannelChangeCb facilities in the @c Embankment class.
 *
 *  Examples of a network resource becoming non-existent include when the underlying
 *  TCP connection has gone away and the IO handler destroyed, or the owning
 *  network resource (e.g. TCP Acceptor or TCP Connector or UDP Sender) has been stopped 
 *  or destroyed and the associated IO handler(s) destroyed. If a @c OutputChannel method 
 *  is called while the "weak pointer" is invalid, an exception will be thrown.
 *
 *  Note that except for the output queue size management callback, callback functor 
 *  registration is only provided at the @c Embankment level, not the individual 
 *  @c OutputChannel level.
 *
 *  An @c OutputChannel object is always created for application use by @c SockLib in a 
 *  @c ChannelChangeCb callback. Similar to the @c Embankment class, if a
 *  @c OutputChannel object is default constructed, it must first have a valid
 *  @c OutputChannel object copied into it before it can be used.
 *
 *  Equivalence and ordering operators are provided to allow @c OutputChannel objects
 *  to be conveniently used in associative or sequence containers, such as @c std::map or
 *  @c std::list.
 *
 *  All @c OutputChannel methods can be called concurrently from multiple threads, with
 *  the following considerations: Modifications are always performed within the context
 *  of the event loop thread. Methods that involve modifications post handlers to the
 *  event loop thread to perform the modifications. This includes queueing buffers for
 *  sending, and modifying output queue policies.
 *
 *  @ingroup SockLibModule
 *
 *  @note Copy constructor, copy assignment operator, and destructor are all
 *  implicit (compiler generated).
 */

class OutputChannel {
public:

/**
 *  @brief Default construct a @c OutputChannel object.
 *
 *  A default constructed @c OutputChannel object is provided for application convenience.
 *  If default constructed, @c OutputChannel methods will throw an exception until 
 *  a valid @c OutputChannel is copied into the default constructed object.
 *  
 */
  OutputChannel () : mResourcePtr() { }

/**
 *  @brief Construct a @c OutputChannel object with an internal network resource.
 *
 *  This constructor is used by @c SockLib internal facilities. It is not meant to be
 *  used by application code.
 *
 *  @note At some point this constructor will be declared @c private, with @c friend
 *  access to appropriate facilities.
 *
 */
  OutputChannel(detail::OutputChannelResourceWeakPtr p) : mResourcePtr(p) { }

/**
 *  @brief Query whether this object is valid (associated with a network resource).
 *
 *  Even though a network association is valid, the network resource may not be in
 *  a good state for further operations. Performing sends (or other operations) on
 *  a network resource that is being shutdown or closed may result in undefined
 *  behavior.
 *
 *  @return @c true if associated network resource is still valid.
 */
  bool valid() const { return !mResourcePtr.expired(); }

/**
 *  @brief Send a message through this @c OutputChannel object.
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
 *  @brief Send a message through this @c OutputChannel object with a 
 *  reference-counted const buffer.
 *
 *  Send a message with a reference-counted const buffer of data. This allows
 *  @c send to proceed without needing to first copy the send
 *  buffer into a @c OutputChannel internal buffer. @c send is a non-blocking call.
 *
 *  See corresponding docs on @c send about UDP IO handler.
 *
 *  @param buf @c shared_const_buffer containing message. Reference counts will
 *  be updated, but no buffer copying performed.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void send(boost_adjunct::shared_const_buffer buf) const {
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
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
 *  incoming datagram can be obtained with the @c OutputChannel @c remoteUdpEndpoint 
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
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
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
 *  an @c IncomingMsgCb. The @c OutputChannel @c stop method is specifically intended for 
 *  TCP Acceptor connections, where a single connection needs to be brought down outside 
 *  of incoming message callback processing. 
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void stop() const {
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
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
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
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
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
      boost::asio::ip::udp::endpoint endp;
      p->remoteUdpEndpoint(endp);
      return endp;
      // return p->remoteUdpEndpoint();
    }
    throw SockLibResourceException("OutputChannel.remoteUdpEndpoint");
  }

/**
 *  @brief Provide a callback that will be invoked if the maximum output queue
 *  size is exceeded.
 *
 *  There are two values for output queue size: 1) the number of entries
 *  on the output queue (each @c OutputChannel @c send method call may result 
 *  in a queue entry, if the message cannot be immediately sent); 2) the number 
 *  of bytes in the output queue (i.e. the sum of the sizes of all output queue 
 *  entries). Each of these have a maximum that can be specified through this 
 *  method. A zero value is a "don't care" value, allowing the value to be as 
 *  big as system resources allow.
 *
 *  Invocation of the callback allows application code to do whatever is 
 *  appropriate, including reporting of the queue sizes or turning off
 *  output buffer sending. Note that an output queue size being exceeded does
 *  <b><i>not</i></b> disable output processing with the @c SockLib
 *  network resources.
 *
 *  @param outQueueSzCb @c OutputQueueSizeCb that will be invoked when either
 *  of the two maximum values are exceeded. If the callback is invoked, both
 *  of the current queue values (number of entries and number of bytes) will 
 *  be provided in the callback.
 *
 *  @param maxEntries Maximum number of entries allowed on the output queue
 *  before the @c OutputQueueSizeCb is invoked (0 means unlimited).
 *
 *  @param maxBytes Maximum number of bytes allowed on the output queue
 *  before the @c OutputQueueSizeCb is invoked (0 means unlimited).
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  void outputQueueSize(OutputQueueSizeCb outQueueSzCb, std::size_t maxEntries = 0, std::size_t maxBytes = 0) {
    maxEntries = maxEntries == 0 ? std::numeric_limits<std::size_t>::max() : maxEntries;
    maxBytes = maxBytes == 0 ? std::numeric_limits<std::size_t>::max() : maxBytes;
    if (detail::OutputChannelResourcePtr p = mResourcePtr.lock()) {
      p->outputQueueSize(outQueueSzCb, maxEntries, maxBytes);
      return;
    }
    throw SockLibResourceException("OutputChannel.outputQueueSizeCb");
  }

  friend bool operator==(const OutputChannel&, const OutputChannel&);
  friend bool operator<(const OutputChannel&, const OutputChannel&);

private:
  detail::OutputChannelResourceWeakPtr mResourcePtr;
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
  detail::OutputChannelResourcePtr plhs = lhs.mResourcePtr.lock();
  detail::OutputChannelResourcePtr prhs = rhs.mResourcePtr.lock();
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
  detail::OutputChannelResourcePtr plhs = lhs.mResourcePtr.lock();
  detail::OutputChannelResourcePtr prhs = rhs.mResourcePtr.lock();
  return (plhs && prhs && plhs < prhs) || (!plhs && prhs);
}

} // end net namespace
} // end chops namespace

#endif

