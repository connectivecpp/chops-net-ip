/** @file 
 *
 *  @defgroup net_ip_module Classes and functions for the Chops Net IP library.
 *
 *  @ingroup net_ip_module
 *
 *  @brief Chops @c net_ip networking class and related functions and facilities.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef NET_IP_HPP_INCLUDED
#define NET_IP_HPP_INCLUDED

#include <memory> // for std::shared_ptr
#include <cstddef> // for std::size_t
#include <string>

#include <experimental/io_context>

#include "net_ip/net_ip_error.hpp"
#include "net_ip/net_entity.hpp"

namespace chops {
namespace net {

/**
 *  @brief Primary class for the Chops Net IP library, initial API point 
 *  for providing TCP acceptor, TCP connector, UDP unicast, and UDP 
 *  multicast capabilities.
 *
 *  A @c net_ip object creates and manages network related objects. It is
 *  the initial API point for creating a TCP acceptor, TCP connector,
 *  UDP unicast, or UDP multicast network entity. Once one of these
 *  network entity objects is created internally, a @c net_entity
 *  object is returned to the application, allowing further operations
 *  to occur.
 *
 *  The @c net_ip class is safe for multiple threads to use concurrently.
 *  Internally operations are posted to a @c std::experimental::net::io_context
 *  for handling within a single thread.
 *
 *  It should be noted, however, that race conditions are possible,
 *  specially for some operations invoked between @c net_entity and
 *  @c net_ip and @c io_interface objects. For example, starting and 
 *  stopping network entities concurrently between separate objects 
 *  or threads could cause unexpected or undefined behavior.
 *
 *  Applications typically  need to interact directly with a @c net_ip object for
 *  network resource operations. Instead, @c net_ip creates facade-like 
 *  objects of type @c Embankment and @c OutputChannel, and application 
 *  functionality uses these to perform networking related operations. 
 *  These operations include sending data, providing various callback and 
 *  message protocol objects, and starting and stopping network processing
 *  on a particular network resource. 
 *
 *  The general application usage pattern for the @ net_ip, @ Embankment, and
 *  @c OutputChannel classes is:
 *
 *  1. Create a @c net_ip object.
 *
 *  2. Create @c Embankment objects, through one of the @c net_ip @c create 
 *  methods. These will be one of either TCP acceptor, TCP connector, UDP 
 *  receiver or sender, UDP multicast receiver or sender, or UDP broadcast 
 *  receiver or sender. 
 *
 *  3. Start processing on the @c Embankment objects, through the @c start method. 
 *  The @c start method is where a @c MsgFrame function object and an 
 *  @c IncomingMsgCb callback are supplied by the application to the @c net_ip 
 *  facilities. A @c ChannelChangeCb callback is also provided by the application 
 *  in the @c start method call for channel change events.
 *
 *  Internally the @c Embankment @c start will cause operations such as socket port 
 *  binding, TCP connect attempts, TCP listens, etc. Part of the @c start method call 
 *  interface is providing a @c ChannelChangeCb callback that will be invoked when a 
 *  TCP connection happens, or the UDP bind completes, or whatever is appropriate for 
 *  the underlying network resource of the @c Embankment object. The callback provides 
 *  a @c OutputChannel object to the application for data sending, at the
 *  appropriate time (e.g. when a connection succeeds).
 *
 *  4. When a @c OutputChannel object is provided to the application, data transfer
 *  can be initiated through @c send method calls for outgoing data, and @c IncomingMsgCb 
 *  callback invocations for incoming data.
 *
 *  5. Call the @c runEventLoop method on the @c net_ip object to enable
 *  event processing on all network resources to occur.
 *
 *  6. Call the @c stop method on the @c net_ip object, which will call the
 *  @c Embankment @c stop for each network resource.
 *
 *  7. Call the @c endEventLoop method on the @c net_ip object.
 *
 *  Network processing for a particular network resource can be started or stopped 
 *  multiple times through the @c Embankment object as needed.
 *
 *  A Boost @c thread object can be easily created to run the @c net_ip event
 *  loop. Example:
 *
 *  @code
 *
 *  socklib::SockLib slib;
 *  boost::thread thr(boost::bind(&socklib::SockLib::runEventLoop, boost::ref(slib)));
 *  // create embankments, start them
 *  slib.stop(); // gracefully stop all network resources
 *  slib.endEventLoop();
 *  thr.join(); // wait for event loop thread to exit
 *
 *  @endcode
 *
 *  Calling the @c stop method on an @c Embankment object will shutdown the 
 *  associated network resource. At this point, other @c Embankment objects 
 *  copied from the original will be affected, and exceptions may be thrown on 
 *  most methods called.
 *
 *  Currently there is not a way to explicitly destroy the internal network 
 *  resource objects associated with @c Embankment objects. They are implicitly 
 *  destroyed once the @c net_ip object goes out of scope. Future enhancements 
 *  may provide this functionality.
 *  
 *  Central to the @c net_ip library is the concept of an application-supplied
 *  @c MsgFrame object. For more information, see documentation associated with the
 *  @c SockLibCallbackDecls.h file.
 *
 *  Internally, the @c Boost @c Asio library is used.
 *
 *  The @c net_ip class does not internally use a Singleton, so multiple @c net_ip
 *  objects can be created as needed or desired. @c net_ip objects cannot
 *  be copied.
 *
 */
class net_ip {
private:
  using tcp_acceptor_ptr = std::shared_ptr<detail::tcp_acceptor>;
  using tcp_connector_ptr = std::shared_ptr<detail::tcp_connector>;
  using udp_entity_ptr = std::shared_ptr<detail::udp_entity_io>;

private:

  std::experimental::net::io_context m_ioc;
  std::vector<tcp_acceptor_ptr> m_acceptors;
  std::vector<tcp_connector_ptr> m_connectors;
  std::vector<udp_entity_ptr> m_udp_entities;

public:

  net_ip() = delete;
  net_ip(const net_ip&) = delete;
  net_ip(net_ip&&) = delete;
  net_ip& operator=(const net_ip&) = delete;
  net_ip& operator=(net_ip&&) = delete;

/**
 *  @brief Construct a @c net_ip object, creating an internal
 *  service object, but not starting any specific network processing.
 *
 *  @param ioc IO context for asynchronous operations.
 */
  explicit net_ip(std::experimental::net::io_context& ioc) :
    m_ioc(ioc), m_acceptors(), m_connectors(), m_udp_entities() { }

/**
 *  @brief Make a TCP acceptor @c net_entity, which will listen on a port for incoming
 *  connections (once it is started).
 *
 *  @param localPort Port number to bind to for incoming TCP connects.
 *
 *  @param listenIntf If this parameter is supplied, the bind will be performed on a
 *  specific IP interface. Otherwise, the bind is for "any" IP interface (which is the
 *  typical usage).
 *
 *  @return @c Embankment object.
 *
 *  @throw @c SockLibException is thrown if host name cannot be resolved.
 *
 */
  tcp_acceptor_net_entity make_tcp_acceptor (

    return tcp_acceptor_net_entity(std::make_shared_ptr<detail::tcp_acceptor>(lkdsfjlkdjf));


  Embankment createTcpAcceptor(unsigned short localPort, const std::string& listenIntf = "", bool noDelay = false) {
    boost::asio::ip::tcp::endpoint endp = createEndpoint<boost::asio::ip::tcp>(listenIntf, localPort, mService);
    detail::SockLibResourcePtr rp(new detail::TcpAcceptor(mService, endp, noDelay));
    mService.post(boost::bind(&SockLib::addResource, this, rp));
    return Embankment(rp);
  }

/**
 *  @brief Create a TCP connector, which will perform an active TCP
 *  connect to the specified address, once started.
 *
 *  A TCP connector performs connects to a remote host on a port with an active listener.
 *  A TCP connector is sometimes called a "client" or "the client side".
 *  This method creates an @c Embankment object, where further processing is to be
 *  performed. When the application is ready to perform the connect to the
 *  remote host, the @c start method on the @c Embankment is called.
 *
 *  A reconnect timeout can be provided, which will result in another connect
 *  attempt (after the timeout period). Reconnect attempts will continue until
 *  a connect is successful or the resource is stopped (through the @c Embankment @c stop 
 *  method). If a connection is broken or the TCP connector is stopped, reconnects will 
 *  not be attempted, so it is the application's responsibility to call @c start again 
 *  on the @c Embankment. 
 *
 *  @param remotePort Port number of remote host.
 *
 *  @param remoteHost Remote host name.
 *
 *  @param reconnTimeMillis Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @param noDelay If @c true, set TCP_NODELAY socket option. Defaults to @c false.
 *
 *  @return @c Embankment object.
 *
 *  @throw @c SockLibException is thrown if host name cannot be resolved.
 */
  Embankment createTcpConnector(unsigned short remotePort, const std::string& remoteHost,
                                std::size_t reconnTimeMillis = 0, bool noDelay = false) {
    boost::asio::ip::tcp::endpoint endp = createEndpoint<boost::asio::ip::tcp>(remoteHost, remotePort, mService);
    detail::SockLibResourcePtr rp(new detail::TcpConnector(mService, endp, reconnTimeMillis, noDelay));
    mService.post(boost::bind(&SockLib::addResource, this, rp));
    return Embankment(rp);
  }

/**
 *  @brief Create a UDP resource that allows both receiving and sending.
 *
 *  When incoming UDP datagrams need to be received, this create method is used. A local port 
 *  is required for binding, and a local host address can also be supplied for binding to 
 *  a specific interface.
 *
 *  If the multicast flag is set, a multicast join will be attempted, using the multicast
 *  address parameter.
 *
 *  Determining whether an incoming UDP datagram was originally sent as unicast, multicast, or 
 *  broadcast is up to the application (by inspecting the remote address through the
 *  @c OutputChannel object, possibly from within the @c IncomingMsgCb).
 *
 *  Note that sending broadcast UDP is not supported through this network resource. Instead, use
 *  the UDP sender create method with the broadcast flag set to @c true.
 *
 *  Binds and reads will not be started until the @c Embankment @c start method is called.
 *
 *  @param localPort Port number for local binding.
 *
 *  @param defRemotePort Port number of default remote host. If left out, the remote port
 *  can be specified as an @c OutputChannel @c send method parameter. Specifying this and
 *  the default remote host allows @c OutputChannel @c send methods to be called without
 *  a destination port and address.
 *
 *  @param defRemoteHost Name of default remote host. Similar to
 *  the default remote port, it can also be specified in a @c send method parameter.
 *
 *  @param mcast If @c true, sets the multicast join socket option on. This allows UDP datagrams
 *  addressed to a multicast address to be received. The reusable socket option is set to allow 
 *  multiple applications on one host to receive incoming multicast UDP datagrams on the same 
 *  address.
 *
 *  @param mcastAddr If the multicast flag is set to @c true, this parameter specifies the
 *  multicast IP address that is to be used for receiving datagrams. If the multicast flag
 *  is @c true and this parameter is empty, an exception is thrown.
 *
 *  @param ttl Time to live (num hops) value. If 0 or not specified, the @c ttl socket option
 *  is not set. If greater than 0, the value is used in the socket option. There is both
 *  a multicast and unicast TTL, and the appropriate socket option is set, depending on the 
 *  multicast flag parameter.
 *
 *  @param localHost If this parameter is supplied, the local bind will be performed on a
 *  specific local host interface. This can be useful in a dual NIC environment for receiving 
 *  datagrams on a specific interface, and may also be useful when sending datagrams. If not
 *  specified, the local bind is for "any" interface (which is the typical usage).
 *
 *  @throw @c SockLibException is thrown if host names cannot be resolved, or if multicast
 *  is specified but the multicast address is empty.
 */
  Embankment createUdpResource(unsigned short localPort, unsigned short defRemotePort = 0,
                       const std::string& defRemoteHost = "", bool mcast = false,
                       const std::string& mcastAddr = "", unsigned short ttl = 0,
                       const std::string& localHost = "") {
    if (mcast && mcastAddr.empty()) {
      throw SockLibException(std::string("Multicast specified but multicast address is empty"));
    }
    return utilityCreateUdpResource(localPort, localHost, defRemotePort, defRemoteHost, true, 
                                    mcast, mcastAddr, false, ttl);
  }

/**
 *  @brief Create a UDP sender network resource, with no associated UDP reads or local
 *  port binding.
 *
 *  When UDP reads are not needed (whether unicast, multicast, or broadcast), a UDP sender resource 
 *  can be created. This simplifies the application code, and does not require a local
 *  port number.
 *
 *  UDP datagrams can be sent to a unicast, multicast, or broadcast address. The broadcast
 *  flag must be specified to allow broadcast UDP packets to be sent.
 *
 *  It is the application responsibility to specify the destination address and port
 *  correctly. In particular, multicast addresses require special network configurations
 *  and assigned IP addresses, and broadcast addresses usually are specified as a directed
 *  broadcast.
 *
 *  Note that destination addresses and ports can either be specified at @c create time (through
 *  this method), or can be specified at each individual @c send method call in the 
 *  @c OutputChannel object.
 *
 *  The @c MsgFrame and @c IncomingMsgCb parameters of the @c Embankment @c start method
 *  are ignored for a UDP sender resource.
 *
 *  @param defRemotePort Port number of default remote host. If left out, the remote port
 *  can be specified as an @c OutputChannel @c send method parameter.
 *
 *  @param defRemoteHost Name of default remote host. Similar to
 *  the default remote port, it can also be specified in a @c send method parameter. Note that for
 *  broadcast UDP, this must be a broadcast address.
 *
 *  @param bcast If @c true, sets the broadcast socket option on. This allows UDP datagrams
 *  addressed to a broadcast address to be sent.
 *
 *  @param ttl Time to live (num hops) value. See documentation in @c createUdpResource.
 *
 *  @param localHost If this parameter is supplied, the local bind will be performed on a
 *  specific local host interface. This can be useful in a dual NIC environment where 
 *  datagrams must only be sent out through a specific interface. Note that there is not a 
 *  facility for specifying a port, since this @c Embankment will only be used for sending and not
 *  receiving.
 *
 *  @param mcast If @c true, this flag allows an outbound interface to be bound, using the localHost
 *  address parameter. It does not perform a multicast join.
 *
 *  @throw @c SockLibException is thrown if host names cannot be resolved.
 */
  Embankment createUdpSender(unsigned short defRemotePort = 0, const std::string& defRemoteHost = "",
                             bool bcast = false, unsigned short ttl = 0,
                             const std::string& localHost = "", bool mcast = false) {
    return utilityCreateUdpResource(0, localHost, defRemotePort, defRemoteHost, false, mcast, "", bcast, ttl);
  }

/**
 *  @brief Perform event multiplexing (connection management, socket input, socket output), 
 *  and callback processing.
 *
 *  This method performs connection management, callback processing, input and output network 
 *  processing, and must be invoked for any of it to occur. This method blocks until 
 *  @c endEventLoop is invoked (e.g. by an @c IncomingMsgCb callback, or by a separate thread).
 *
 *  This method can be ignored if the @c io_service object's @c run method is called from
 *  elsewhere.
 *
 *  This method creates an @c asio::io_service::work object, so that it can be called at any
 *  time, even if there are no event handlers (from network resources) created or started
 *  (through an @c Embankment @c start). When @c endEventLoop is called, the 
 *  @c asio::io_service::work object will be destroyed, and
 *  the event loop will exit when all event handlers are finished running (all reads, writes,
 *  and timers are completed or cancelled).
 *
 *  @note For now, only a single thread can call @c runEventLoop or the @c io_service @c run 
 *  method. Future enhancements may allow multiple threads to call these methods, protecting 
 *  concurrent access through Asio @c strand objects. The granularity of the concurrency will 
 *  be for each network resource, as created through a @c create method on @c SockLib. In 
 *  other words, a given network resource will be serialized on its asynchronous operations, 
 *  to eliminate chances of handlers for a given network resource being invoked in separate threads.
 *
 *  See the concurrency notes in the Boost Asio documentation, in the overview section and
 *  on the documentation of the @c asio::io_service::run method.
 *
 */
  void runEventLoop () {
    mWorker = WorkerPtr(new boost::asio::io_service::work(mService));
    mService.run();
  }

/**
 *  @brief Destroy the internal worker object and allow the event loop to end.
 *
 *  This method will destroy the internal work object, and when there are no
 *  additional network resource event handlers in progress, allows the @c runEventLoop
 *  method to exit.
 *
 *  The @c stop method should be called before this method is invoked, to allow
 *  the network resources to gracefully shutdown.
 */
  void endEventLoop() {
    mWorker.reset();
  }

/**
 *  @brief Stop network processing on all network resources.
 *
 *  Stop all @c Embankment objects in a graceful manner. This allows the event loop to
 *  be cleanly exited, and the @c SockLib object to be destroyed.
 *
 */
  void stop() {
    mService.post(boost::bind(&SockLib::stopResources, this));
  }

private:

  Embankment utilityCreateUdpResource(unsigned short locPort, const std::string& locHost,
                                      unsigned short defRemPort, const std::string& defRemHost,
                                      bool startRead, bool mcast, const std::string& mcastAddr,
                                      bool bcast, unsigned short ttl) {
    boost::asio::ip::udp::endpoint locEndp = createEndpoint<boost::asio::ip::udp>(locHost, locPort, mService);
    boost::asio::ip::udp::endpoint defRemEndp = createEndpoint<boost::asio::ip::udp>(defRemHost, defRemPort, mService);
    boost::asio::ip::udp::endpoint mcastEndp = mcast ? createEndpoint<boost::asio::ip::udp>(mcastAddr, 0, mService) :
                                               boost::asio::ip::udp::endpoint();
    detail::SockLibResourcePtr rp(new detail::UdpResource(mService, locEndp, defRemEndp,
                                                          startRead, mcast, mcastEndp, bcast, ttl));
    mService.post(boost::bind(&SockLib::addResource, this, rp));
    return Embankment(rp);
  }

  // following methods invoked inside event loop thread
  void addResource(detail::SockLibResourcePtr rp) {
    mResources.push_back(rp);
  }

  void stopResources() {
    for (ResourcesIter iter = mResources.begin(); iter != mResources.end(); ++iter) {
      (*iter)->stop();
    }
  }

private:

  std::auto_ptr<boost::asio::io_service>      mServicePtr;
  boost::asio::io_service&                    mService;
  typedef std::list<detail::SockLibResourcePtr>   Resources;
  typedef Resources::iterator                     ResourcesIter;
  Resources                                   mResources;
  typedef std::auto_ptr<boost::asio::io_service::work> WorkerPtr;
  WorkerPtr                                   mWorker;

};

}  // end net namespace
}  // end chops namespace

#endif

