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
#include <string_view>
#include <vector>

#include <experimental/io_context>
#include <experimental/executor>

#include "net_ip/net_ip_error.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip/make_endpoints.hpp"
#include "utility/erase_where.hpp"

namespace chops {
namespace net {

/**
 *  @brief Primary class for the Chops Net IP library and the initial API 
 *  point for providing TCP acceptor, TCP connector, UDP unicast, and UDP 
 *  multicast capabilities.
 *
 *  A @c net_ip object creates and manages network related objects. It is
 *  the initial API point for creating a TCP acceptor, TCP connector,
 *  UDP unicast, or UDP multicast network entity. Once one of these
 *  network objects is created internal to the @c net_ip object, a @c net_entity 
 *  object is returned to the application, allowing further operations to occur.
 *
 *  Applications perform most operations with either a @c net_entity or a 
 *  @c io_interface object. The @c net_ip object creates facade-like objects of 
 *  type @c net_entity, which allow further operations.
 *
 *  The general application usage pattern for the @ net_ip, @ net_entity, and
 *  @c io_interface classes is:
 *
 *  1. Instantiate a @c net_ip object.
 *
 *  2. Create a @c net_entity object, through one of the @c net_ip @c make 
 *  methods. A @c net_entity interacts with one of a TCP acceptor, TCP 
 *  connector, UDP unicast receiver or sender, or UDP multicast receiver (a
 *  UDP multicast sender is the same as a UDP unicast sender).
 *
 *  3. Call the @c start method on the @c net_entity object. This performs
 *  name resolution (if needed), a local bind (if needed) and (for TCP) a 
 *  connect or a listen. 
 *
 *  If name resolution (i.e. DNS lookup) is required on a host name, the 
 *  name resolution starts when @c start is called. If this is not acceptable,
 *  the application can perform the lookup and the endpoint (or endpoint 
 *  sequence) can be passed in through the @c make method.
 *
 *  The @c start method takes a state change function object, and this will 
 *  be called when the @c net_entity becomes ready for network input and 
 *  output. The state change callback will be invoked if an error occurs.
 *
 *  4. When an @c io_interface object is supplied to the application through
 *  the state change callback, input processing is started through a @c start_io
 *  call, and outbound data is sent through @c send methods.
 *
 *  There are no executor operations available through the @c net_ip class. In 
 *  other words, no event loop or @c run methods are available. Instead, the
 *  @c net_ip class takes an @c io_context as a constructor parameter and 
 *  application code will use the Networking TS executor methods for invoking
 *  the underlying asynchronous operations.
 *
 *  For convenience, a class named @c worker combines an executor with a work
 *  guard and creates a thread to invoke the asynchronous operations. Example
 *  usage:
 *
 *  @code
 *    chops::net::worker wk;
 *    wk.start();
 *    chops::net::net_ip my_nip(wk.get_io_context());
 *    // ...
 *    wk.stop(); // or wk.reset();
 *  @endcode
 *
 *  The @c net_ip class is safe for multiple threads to use concurrently. 
 *  (Internally function objects are posted through the 
 *  @c std::experimental::net::io_context for handling within a single thread 
 *  or strand context.)
 *
 *  It should be noted, however, that race conditions are possible, specially for 
 *  similar operations invoked between @c net_entity and @c net_ip and @c io_interface 
 *  objects. For example, starting and stopping network entities concurrently between 
 *  separate objects or threads could cause unexpected or undefined behavior.
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
 *  @brief Construct a @c net_ip object without starting any network processing.
 *
 *  @param ioc IO context for asynchronous operations.
 */
  explicit net_ip(std::experimental::net::io_context& ioc) :
    m_ioc(ioc), m_acceptors(), m_connectors(), m_udp_entities() { }

/**
 *  @brief Create a TCP acceptor @c net_entity, which will listen on a port for incoming
 *  connections (once started).
 *
 *  @param local_port Port number to bind to for incoming TCP connects.
 *
 *  @param listen_intf If this parameter is supplied, the bind will be performed on a
 *  specific IP interface. Otherwise, the bind is for "any" IP interface (which is the
 *  typical usage).
 *
 *  @return @c tcp_acceptor_net_entity object.
 *
 */
  tcp_acceptor_net_entity make_tcp_acceptor (std::string_view local_port, std::string_view listen_intf = "") {
    tcp_acceptor_ptr p = std::make_shared<detail::tcp_acceptor>(m_ioc, local_port, listen_intf);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_acceptors.push_back(p); } );
    return tcp_acceptor_net_entity(p);
  }

/**
 *  @brief Create a TCP acceptor @c net_entity, using an already created endpoint.
 *
 *  This allows flexibility in creating an endpoint for the acceptor to use.
 *
 *  @param endp A @c std::experimental::net::ip::tcp::endpoint that the acceptor uses for the local
 *  bind.
 *
 *  @return @c tcp_acceptor_net_entity object.
 *
 */
  tcp_acceptor_net_entity make_tcp_acceptor (const std::experimental::net::ip::tcp::endpoint& endp) {
    tcp_acceptor_ptr p = std::make_shared<detail::tcp_acceptor>(m_ioc, endp);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_acceptors.push_back(p); } );
    return tcp_acceptor_net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity, which will perform an active TCP
 *  connect to the specified address (once started).
 *
 *  Internally a sequence of endpoints will be looked up through a name resolver,
 *  and each endpoint will be tried in succession.
 *
 *  If a reconnect timeout is provided (parm > 0), connect failures result in reconnect 
 *  attempts after the timeout period. Reconnect attempts will continue until a connect is 
 *  successful or the @c net_entity @c stop method is called. If a connection is broken or the 
 *  TCP connector is stopped, reconnects will not be attempted, so it is the application's 
 *  responsibility to call @c start again on the @c net_entity. 
 *
 *  @param remote_port_or_service Port number or service name of remote host.
 *
 *  @param remote_host Remote host name.
 *
 *  @param reconn_time_millis Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @return @c tcp_connector_net_entity object.
 *
 */
  tcp_connector_net_entity make_tcp_connector (std::string_view remote_port_or_service,
                                               std::string_view remote_host,
                                               std::size_t reconn_time_millis = 0) {

    tcp_connector_ptr p = std::make_shared<detail::tcp_connector>(m_ioc, remote_port_or_service, 
                                                                  remote_host, reconn_time_millis);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_connectors.push_back(p); } );
    return tcp_connector_net_entity(p);


/**
 *  @brief Create a TCP connector @c net_entity, using an already created sequence of 
 *  endpoints.
 *
 *  This allows flexibility in creating the endpoints for the connector to use.
 *
 *  @param beg A begin iterator to a sequence of @c std::experimental::net::ip::tcp::endpoint
 *  objects.
 *
 *  @param end An end iterator to a sequence of endpoints.
 *
 *  @param reconn_time_millis Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @return @c tcp_connector_net_entity object.
 *
 */

  template <typename Iter>
  tcp_connector_net_entity make_tcp_connector (m_ioc, Iter beg, Iter end, std::size_t reconn_time_millis = 0);
    tcp_connector_ptr p = std::make_shared<detail::tcp_connector>(beg, end, reconn_time_millis);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_connectors.push_back(p); } );
    return tcp_connector_net_entity(p);
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
 *  @c io_interface object, possibly from within the @c IncomingMsgCb).
 *
 *  Note that sending broadcast UDP is not supported through this network resource. Instead, use
 *  the UDP sender create method with the broadcast flag set to @c true.
 *
 *  Binds and reads will not be started until the @c net_entity @c start method is called.
 *
 *  @param localPort Port number for local binding.
 *
 *  @param defRemotePort Port number of default remote host. If left out, the remote port
 *  can be specified as an @c io_interface @c send method parameter. Specifying this and
 *  the default remote host allows @c io_interface @c send methods to be called without
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
 *  @c io_interface object.
 *
 *  The @c MsgFrame and @c IncomingMsgCb parameters of the @c net_entity @c start method
 *  are ignored for a UDP sender resource.
 *
 *  @param defRemotePort Port number of default remote host. If left out, the remote port
 *  can be specified as an @c io_interface @c send method parameter.
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
 *  facility for specifying a port, since this @c net_entity will only be used for sending and not
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
 *  @brief Call @c stop on a TCP acceptor @c net_entity and remove it from the internal
 *  list of TCP acceptors.
 *
 *  @param acc TCP acceptor @c net_entity to be stopped and removed.
 *
 */
  void remove(tcp_acceptor_net_entity acc) {
    std::experimental::net::post(m_ioc.get_executor(), 
          [acc, this] () { acc->stop(); chops::erase_where(m_acceptors, acc); } );
  }

/**
 *  @brief Call @c stop on a TCP connector @c net_entity and remove it from the internal
 *  list of TCP connectors.
 *
 *  @param conn TCP connector @c net_entity to be stopped and removed.
 *
 */
  void remove(tcp_connector_net_entity conn) {
    std::experimental::net::post(m_ioc.get_executor(), 
          [conn, this] () { conn->stop(); chops::erase_where(m_connectors, acc); } );
  }

/**
 *  @brief Call @c stop on a UDP @c net_entity and remove it from the internal
 *  list of UDP entities.
 *
 *  @param udp_ent UDP @c net_entity to be stopped and removed.
 *
 */
  void remove(udp_net_entity udp_ent) {
    std::experimental::net::post(m_ioc.get_executor(), 
          [udp_ent, this] () { udp_ent->stop(); chops::erase_where(m_udp_entities, udp_ent); } );
  }

/**
 *  @brief Call @c stop and remove all acceptors, connectors, and UDP entities.
 *
 *  This method allows for a more measured shutdown, if needed.
 *
 */
  void remove_all() {
    std::experimental::net::post(m_ioc.get_executor(), [this] () {
        for (auto i : m_acceptors) { i->stop(); }
        for (auto i : m_connectors) { i->stop(); }
        for (auto i : m_udp_entities) { i->stop(); }
        m_acceptors.clear();
        m_connectors.clear();
        m_udp_entities.clear();
      }
    );
  }

};

}  // end net namespace
}  // end chops namespace

#endif

