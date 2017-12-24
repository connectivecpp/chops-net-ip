/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c net_entity class and related functionality.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef NET_ENTITY_HPP_INCLUDED
#define NET_ENTITY_HPP_INCLUDED

#include "Socket/SockLibException.h"

#include <cstddef> // std::size_t 

namespace chops {
namespace net {

/**
 *  @brief The @c net_entity class is the primary application interface into the
 *  @c SockLib facilities, including callback object interfaces and start / stop 
 *  processing.
 *
 *  The @c net_entity class provides callback object interfaces for a 
 *  @c SockLib facility, as well as ways to control processing (start, stop,
 *  validity querying). An @c net_entity object is the primary way that an
 *  application interfaces to @c SockLib, outside of the @c OutputChannel class,
 *  which is used for message sending.
 *
 *  The @c net_entity class is a lightweight, value-based class, designed
 *  to be easy and efficient to copy and store as a member object. It 
 *  abstracts multiple network protocols into a consistent interface.
 *  It does not contain any network resources itself, but instead provides a 
 *  "weak pointer" to the actual network resource that is managed inside
 *  a @c SockLib object.
 *
 *  A "network resource" is a @c SockLib internal object that is a TCP
 *  acceptor or connector, or a UDP sender or receiver, or a multicast
 *  sender or receiver, or a broadcast sender or receiver. Each of these
 *  are created through a corresponding @c SockLib @c create method (see
 *  @c SockLib class for additional details).
 *
 *  An @c net_entity can either point to a valid network resource (i.e.
 *  the "weak pointer" is good), or can point to a non-existent or invalid
 *  network resource (i.e. the "weak pointer" is invalid). A non-existent
 *  or invalid network resource might occur if the network resource has
 *  been destroyed (through the @c SockLib interface), or the @c SockLib
 *  object itself has been stopped or destroyed. If an @c net_entity method 
 *  is called while the "weak pointer" is invalid, an exception will be thrown.
 *
 *  If an @c net_entity is default constructed, it is in an invalid state
 *  until another @c net_entity object is copied into it. The @c create
 *  methods of the @c SockLib class (factory functions) provide an @c net_entity 
 *  that is fully valid and ready to use.
 *
 *  Equivalence and ordering operators are provided to allow @c net_entity objects
 *  to be conveniently used in associative or sequence containers, such as @c std::map or
 *  @c std::list.
 *
 *  All @c net_entity methods are safe to call concurrently from multiple
 *  threads. However, care must be taken when calling @c net_entity
 *  methods from within a callback, such as a @c ChannelChangeCb or
 *  an @c IncomingMsgCb.
 *
 *  @ingroup SockLibModule
 *
 *  @note Copy constructor, copy assignment operator, and destructor are all
 *  implicit (compiler generated).
 */

class net_entity {
public:

  // net_entity (net_entity const&); // implicit - compiler generated
  // net_entity& operator= (net_entity const&); // implicit - compiler generated
  // ~net_entity(); // implicit - compiler generated

/**
 *  @brief Default construct an @c net_entity object.
 *
 *  A default constructed @c net_entity object is provided for application convenience.
 *  If default constructed, most @c net_entity methods will throw an exception until 
 *  a valid @c net_entity is copied into the default constructed object.
 *  
 */
  net_entity () : mResourcePtr() { }

/**
 *  @brief Construct an @c net_entity object with a @c SockLib resource.
 *
 *  This constructor is used by @c SockLib internal facilities. It is not meant
 *  to be used by application code.
 *
 *  @note At some point this constructor will be declared @c private, with @c friend
 *  access to appropriate facilities.
 *  
 */
  net_entity (detail::SockLibResourceWeakPtr p) : mResourcePtr(p) { }

/**
 *  @brief Query whether this object is valid (associated with a network resource).
 *
 *  An @c net_entity object is associated with a @c SockLib network resource
 *  through the @c SockLib @c create methods. If an @c net_entity is default constructed,
 *  it is not in a valid state, and this method will return @c false. If the 
 *  network resource associated with an @c net_entity is destroyed (e.g. the 
 *  creating @c SockLib object is destroyed, for example by calling the @c net_entity @c stop 
 *  method), this method will return @c false.
 *
 *  @return @c true if associated network resource is still valid.
 */
  bool valid() const { return !mResourcePtr.expired(); }

/**
 *  @brief Query whether the associated network resource is started or not.
 *
 *  The associated network resource is started if the @c start method from an associated
 *  @c net_entity has been called. 
 *
 *  @return @c true if the network resource has been started, @c false otherwise.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 */
  bool started() const {
    if (detail::SockLibResourcePtr p = mResourcePtr.lock()) {
      return p->started();
    }
    throw SockLibResourceException("net_entity.started");
  }

/**
 *  @brief Start network processing on the associated network resource.
 *
 *  Once a network resource is created through a @c SockLib @c create method, it
 *  must be started before processing occurs. Requiring a specific @c start method
 *  to be called allows application control of when the port binding and follow-on
 *  processing is to be started (e.g. TCP connect attempt, TCP listen, TCP reads). 
 *
 *  The @c start method also provides various callback objects to the network
 *  resource. The @c MsgFrame and @c IncomingMsgCb callbacks are provided, as
 *  well as a @c ChannelChangeCb. 
 *
 *  For network resources where reads are not important, a @c start method
 *  with no @c MsgFrame or @c IncomingMsgCb parameters is provided. This might
 *  be a UDP sender, or a TCP acceptor or connector where incoming data is not 
 *  needed.
 *
 *  A network resource can be stopped, and then later started. The @c stop
 *  method must be called before @c start can be called, even if the network
 *  resource is internally in the equivalent of a "stopped" state. An example
 *  of this would be a TCP Connector where the TCP connection has been broken,
 *  and a connect is to be attempted again.
 *
 *  Errors are reported in one of two ways - immediately, through an exception 
 *  thrown from this method, or later on through the @c ChannelChangeCb callback.
 *  Bind errors, or a TCP listen failure, or other similar errors result in
 *  a thrown exception. Errors that happen later on in the network processing
 *  timeline, such as TCP connect error, or an accept error, or a broken TCP 
 *  connection, result in a @c ChannelChangeCb notification.
 *
 *  Bind failures are usually due to a port that is already in use.
 *
 *  @param ccCb @c ChannelChangeCb callback, which will provide a @c OutputChannel 
 *  object and other information when a network resource is ready for input and output. 
 *  A function named @c simpleChannelChangeCb is available for applications that
 *  do not care about notification. The @c ChannelChangeCb is copied at least once 
 *  during @c start method invocation, but never copied afterwards (to simplify 
 *  any internal state management). See @c ChannelChangeCb documentation for more 
 *  info. To allow convenient app state initialization, a @c ChannelChangeCb callback 
 *  will be immediately invoked before any network processing is started, with the 
 *  following values:
 *  1) empty @c OutputChannel (default constructed, @c valid() == @c false); 
 *  2) zero (non-error) boost::system::error_code; 
 *  3) 0 (count of valid connections or resources for this @c net_entity).
 *
 *  @param msgFrame Function object or function pointer that provides "message
 *  framing" callback handling. See @c MsgFrame class documentation for more info.
 *  If no application specific message framing is needed, a default @c MsgFrame
 *  object can be used (of type @c SimpleMsgFrame), which reads one byte at a time. 
 *  A @c SimpleMsgFrame object is also appropriate to pass in for UDP sender 
 *  network resources, where reads are not performed. A copy will be made at 
 *  initial @c start method invocation, but once network processing 
 *  has begun no additional copies will be made (simplifying application logic
 *  regarding internal states of the @c MsgFrame or @c IncomingMsgCb objects).
 *
 *  @param incMsgCb Callback of type @c IncomingMsgCb for processing incoming
 *  messages. See @c IncomingMsgCb for more info. If no incoming message processing 
 *  is needed, the @c simpleIncomingMsgCb function can be used, which does
 *  nothing. A copy will be made at @c start method invocation time, but no copies 
 *  will be made once network processing has started.
 *
 *  @return @c false if the resource is already started, @c true otherwise.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid. A
 *  @c boost::system::system_error is thrown if the immediate start processing 
 *  fails (e.g. port bind error).
 *
 *  @note A copy will be made of the @c MsgFrame and @c IncomingMsgCb objects
 *  before read processing is started. For example, a copy is made each time a TCP
 *  connect is attempted. However, once the read processing is started, no additional
 *  copies will be made. Copying of these objects in an initial state should be
 *  designed to be efficient (and correct), while copying after the initial state
 *  (i.e. once reads are started) does not need the same design rigor.
 */
  template <class MsgFrame>
  bool start(const ChannelChangeCb& ccCb, const MsgFrame& msgFrame, 
             const typename IncomingMsgCb<MsgFrame>::Callback& incMsgCb) {

    if (detail::SockLibResourcePtr p = mResourcePtr.lock()) {
      return p->start(ccCb, msgFrame, incMsgCb);
    }
    throw SockLibResourceException("net_entity.start");
  }

/**
 *  @brief Start network processing on the associated network resource, but use 
 *  @c SimpleMsgFrame and @c simpleIncomingMsgCb for the @c MsgFrame and
 *  @c IncomingMsgCb parameters.
 *
 *  @param ccCb Same as the three parameter @c start.
 *
 *  @return Same as the three parameter @c start.
 */
  bool start(const ChannelChangeCb& ccCb) {
    if (detail::SockLibResourcePtr p = mResourcePtr.lock()) {
      return p->start(ccCb, SimpleMsgFrame(), simpleIncomingMsgCb);
    }
    throw SockLibResourceException("net_entity.start");
  }

/**
 *  @brief Stop processing on the associated network facility and destroy associated 
 *  resources.
 *
 *  Stopping network processing may involve closing connections, deallocating
 *  resources, and unbinding from ports. It will affect other @c net_entity objects
 *  associated with the same network resource.
 *
 *  A @c ChannelChangeCb will be invoked for each associated TCP connection or UDP resource 
 *  when @c stop is called, with the following values:
 *  1) valid @c OutputChannel corresponding to the connection / resource
 *  2) boost::system::error_code with the value boost::system::errc::operation_canceled
 *  3) 0 / N (count of valid connections or resources for this @c net_entity).
 *
 *  Specifically, a TCP connector and UDP resource will have one @c ChannelChangeCb invocation
 *  with a channel count of 0, while a TCP acceptor will have a @c ChannelChangeCb invocation
 *  for each current connection (and there may be 0 connections), and one additional invocation 
 *  with an empty @c OutputChannel and a channel count of 0.
 *
 *  @note Other method calls made to this object after @c stop is invoked
 *  result in an exception thrown, or undefined behavior. The @c valid method may be 
 *  called to query the state.
 *
 *  @throw @c SockLibException is thrown if the underlying resource is invalid.
 *
 */
  void stop() {
    if (detail::SockLibResourcePtr p = mResourcePtr.lock()) {
      p->stop();
      return;
    }
    throw SockLibResourceException("net_entity.stop");
  }

  friend bool operator==(const net_entity&, const net_entity&);
  friend bool operator<(const net_entity&, const net_entity&);
private:

  detail::SockLibResourceWeakPtr mResourcePtr;

};

/**
 *  @brief Compare two @c net_entity objects for equality.
 *
 *  See @c OutputChannel notes for equivalence logic (same logic applies here).
 *
 *  @return @c true if two @c net_entity objects are pointing to the same internal resource.
 *
 *  @relates net_entity
 */

inline bool operator==(const net_entity& lhs, const net_entity& rhs) {
  detail::SockLibResourcePtr plhs = lhs.mResourcePtr.lock();
  detail::SockLibResourcePtr prhs = rhs.mResourcePtr.lock();
  return (plhs && prhs && plhs == prhs) || (!plhs && !prhs);
}

/**
 *  @brief Compare two @c net_entity objects for ordering purposes, allowing @c net_entity objects
 *  to be stored in a @c std::map (or other associative container).
 *
 *  See @c OutputChannel notes for ordering logic (same logic applies here).
 *
 *  @return @c true if left @c net_entity less than right @c net_entity according to internal
 *  resource pointer ordering and invalid object ordering as defined in the comments.
 *
 *  @relates net_entity
 */
inline bool operator<(const net_entity& lhs, const net_entity& rhs) {
  detail::SockLibResourcePtr plhs = lhs.mResourcePtr.lock();
  detail::SockLibResourcePtr prhs = rhs.mResourcePtr.lock();
  return (plhs && prhs && plhs < prhs) || (!plhs && prhs);
}

} // end net namespace
} // end chops namespace

#endif

