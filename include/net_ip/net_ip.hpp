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
#include <chrono>

#include <experimental/io_context>
#include <experimental/executor>
#include <experimental/internet>

#include "net_ip/net_ip_error.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/udp_entity_io.hpp"
#include "net_ip/detail/tcp_io.hpp"

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
 *  Local host and port and interface name lookups are performed immediately 
 *  using direct (synchronous) lookups. Remote host and port name lookups are 
 *  performed asynchronously (since these may take longer) and are started when 
 *  the @c net_entity @c start is called (this is only needed for TCP connectors).
 *  If this is not acceptable, the application can perform the lookup and the 
 *  endpoint (or endpoint sequence) can be passed in through the @c make method.
 *
 *  State change function objects are invoked when network IO can be started as
 *  well as when an error or shutdown occurs.
 *
 *  4. When an @c io_interface object is supplied to the application through
 *  the start state change callback, input processing is started through a 
 *  @c start_io call, and outbound data is sent through @c send methods.
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
 *    wk.reset(); // or wk.stop();
 *  @endcode
 *
 *  The @c net_ip class is safe for multiple threads to use concurrently. 
 *  (Internally function objects are posted through the 
 *  @c std::experimental::net::io_context for handling within a single thread 
 *  or strand context.)
 *
 *  It should be noted, however, that race conditions are possible, specially for 
 *  similar operations invoked between @c net_entity and @c io_interface 
 *  objects. For example, starting and stopping network entities concurrently between 
 *  separate objects or threads could cause unexpected behavior.
 *
 */
class net_ip {
private:

  std::experimental::net::io_context& m_ioc;
  std::vector<detail::tcp_acceptor_ptr> m_acceptors;
  std::vector<detail::tcp_connector_ptr> m_connectors;
  std::vector<detail::udp_entity_io_ptr> m_udp_entities;

public:

/**
 *  @brief Construct a @c net_ip object without starting any network processing.
 *
 *  @param ioc IO context for asynchronous operations.
 */
  explicit net_ip(std::experimental::net::io_context& ioc) :
    m_ioc(ioc), m_acceptors(), m_connectors(), m_udp_entities() { }

private:

  net_ip() = delete;
  net_ip(const net_ip&) = delete;
  net_ip(net_ip&&) = delete;
  net_ip& operator=(const net_ip&) = delete;
  net_ip& operator=(net_ip&&) = delete;

public:

/**
 *  @brief Create a TCP acceptor @c net_entity, which will listen on a port for incoming
 *  connections (once started).
 *
 *  @param local_port_or_service Port number or service name to bind to for incoming TCP 
 *  connects.
 *
 *  @param listen_intf If this parameter is supplied, the bind (when @c start is called) will 
 *  be performed on this specific interface. Otherwise, the bind is for "any" IP interface 
 *  (which is the typical usage).
 *
 *  @param reuse_addr If @c true (default), the @c reuse_address socket option is set upon 
 *  socket open.
 *
 *  @return @c tcp_acceptor_net_entity object.
 *
 *  @throw @c std::system_error if there is a name lookup failure.
 *
 *  @note The name and port lookup to create a TCP endpoint is immediately performed. The
 *  alternate TCP acceptor @c make method can be used if this is not acceptable.
 *
 */
  tcp_acceptor_net_entity make_tcp_acceptor (std::string_view local_port_or_service, 
                                             std::string_view listen_intf = "",
                                             bool reuse_addr = true) {
    endpoints_resolver<std::experimental::net::ip::tcp> resolver(m_ioc);
    auto results = resolver.make_endpoints(true, listen_intf, local_port_or_service);
    return make_tcp_acceptor(results.cbegin()->endpoint(), reuse_addr);
  }

/**
 *  @brief Create a TCP acceptor @c net_entity, using an already created endpoint.
 *
 *  This @c make method allows flexibility in creating an endpoint for the acceptor to use, 
 *  such as directly specifying ipV4 or ipV6 in name resolving, or directly creating the endpoint 
 *  without using name resolving.
 *
 *  @param endp A @c std::experimental::net::ip::tcp::endpoint that the acceptor uses for the local
 *  bind (when @c start is called).
 *
 *  @param reuse_addr If @c true (default), the @c reuse_address socket option is set upon 
 *  socket open.
 *
 *  @return @c tcp_acceptor_net_entity object.
 *
 */
  tcp_acceptor_net_entity make_tcp_acceptor (const std::experimental::net::ip::tcp::endpoint& endp,
                                             bool reuse_addr = true) {
    auto p = std::make_shared<detail::tcp_acceptor>(m_ioc, endp, reuse_addr);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_acceptors.push_back(p); } );
    return tcp_acceptor_net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity, which will perform an active TCP
 *  connect to the specified host and port (once started).
 *
 *  Internally a sequence of remote endpoints will be looked up through a name resolver,
 *  and each endpoint will be tried in succession.
 *
 *  If a reconnect timeout is provided (parm > 0), connect failures result in reconnect 
 *  attempts after the timeout period. Reconnect attempts will continue until a connect is 
 *  successful or the @c net_entity @c stop method is called. If a connection is broken or the 
 *  TCP connector is stopped, reconnects will not be attempted, so it is the application's 
 *  responsibility to call @c start again on the @c net_entity. 
 *
 *  @param remote_port_or_service Port number or service name on remote host.
 *
 *  @param remote_host Remote host name or IP address.
 *
 *  @param reconn_time Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @return @c tcp_connector_net_entity object.
 *
 *  @note The name and port lookup to create a sequence of remote TCP endpoints is not performed
 *  until the @c net_entity @c start method is called. If this is not acceptable, the
 *  endpoints can be looked up by the application and the alternate @c make_tcp_connector
 *  method called.
 *
 */
  tcp_connector_net_entity make_tcp_connector (std::string_view remote_port_or_service,
                                               std::string_view remote_host,
                                               std::chrono::milliseconds reconn_time = 
                                                 std::chrono::milliseconds { } ) {

    auto p = std::make_shared<detail::tcp_connector>(m_ioc, remote_port_or_service, 
                                                                  remote_host, reconn_time);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_connectors.push_back(p); } );
    return tcp_connector_net_entity(p);
  }


/**
 *  @brief Create a TCP connector @c net_entity, using an already created sequence of 
 *  endpoints.
 *
 *  This allows flexibility in creating the remote endpoints for the connector to use.
 *
 *  @param beg A begin iterator to a sequence of remote @c std::experimental::net::ip::tcp::endpoint
 *  objects.
 *
 *  @param end An end iterator to the sequence of endpoints.
 *
 *  @param reconn_time Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @return @c tcp_connector_net_entity object.
 *
 */
  template <typename Iter>
  tcp_connector_net_entity make_tcp_connector (Iter beg, Iter end,
                                               std::chrono::milliseconds reconn_time = 
                                                 std::chrono::milliseconds { } ) {
    auto p = std::make_shared<detail::tcp_connector>(m_ioc, beg, end, reconn_time);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_connectors.push_back(p); } );
    return tcp_connector_net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity using a single remote endpoint.
 *
 *  @param endp Remote @c std::experimental::net::ip::tcp::endpoint to use for the connect
 *  attempt.
 *
 *  @param reconn_time_millis Time period in milliseconds between connect attempts. If 0, no
 *  reconnects are attempted (default is 0).
 *
 *  @return @c tcp_connector_net_entity object.
 *
 */
  tcp_connector_net_entity make_tcp_connector (const std::experimental::net::ip::tcp::endpoint& endp, 
                                               std::chrono::milliseconds reconn_time = 
                                                 std::chrono::milliseconds { } ) {
    std::vector<std::experimental::net::ip::tcp::endpoint> vec { endp };
    return make_tcp_connector(vec.cbegin(), vec.cend(), reconn_time);
  }

/**
 *  @brief Create a UDP unicast @c net_entity that allows receiving as well as sending.
 *
 *  This @c make method is used when incoming UDP (unicast) datagrams will be received. A
 *  local port is used for binding, and an optional local host address can also be used as
 *  part of the bind (e.g. if binding to a specific interface is needed).
 *
 *  If there is a need to determine whether an incoming UDP datagram was originally sent 
 *  as unicast, multicast, or broadcast this can be performed by inspecting the remote 
 *  endpoint address as supplied through the message handler callback.
 *
 *  A bind to the local endpoint is not started until the @c net_entity @c start method is 
 *  called, and a read is not started until the @c io_interface @c start_io method is
 *  called.
 *
 *  @param local_port_or_service Port number or service name for local binding.
 *
 *  @param local_intf Local interface name, otherwise the default is "any address".
 *
 *  @throw @c std::system_error if there is a name lookup failure.
 *
 *  @note Common socket options on UDP datagram sockets, such as increasing the 
 *  "time to live" (hop limit), allowing UDP broadcast, or setting the socket 
 *  reuse flag can be set by using the @c net_entity @c get_socket method (or 
 *  @c io_interface @c get_socket method, which returns the same reference).
 *
 */
  udp_net_entity make_udp_unicast (std::string_view local_port_or_service, 
                                   std::string_view local_intf = "") {
    endpoints_resolver<std::experimental::net::ip::udp> resolver(m_ioc);
    auto results = resolver.make_endpoints(true, local_intf, local_port_or_service);
    return make_udp_unicast(results.cbegin()->endpoint());
  }

/**
 *  @brief Create a UDP unicast @c net_entity for receiving and sending, using an already 
 *  created endpoint.
 *
 *  This @c make method allows flexibility in creating an endpoint for the UDP unicast
 *  @c net_entity to use.
 *
 *  @param endp A @c std::experimental::net::ip::udp::endpoint used for the local bind 
 *  (when @c start is called).
 *
 *  @return @c udp_net_entity object.
 *
 */
  udp_net_entity make_udp_unicast (const std::experimental::net::ip::udp::endpoint& endp) {
    auto p = std::make_shared<detail::udp_entity_io>(m_ioc, endp);
    std::experimental::net::post(m_ioc.get_executor(), [p, this] () { m_udp_entities.push_back(p); } );
    return udp_net_entity(p);
  }

/**
 *  @brief Create a UDP unicast @c net_entity for sending only (no local bind is performed).
 *
 *  This @c make method is used when no UDP reads are desired, only sends.
 *
 *  @return @c udp_net_entity object.
 *
 */
  udp_net_entity make_udp_sender () {
    return make_udp_unicast(std::experimental::net::ip::udp::endpoint());
  }

// TODO: multicast make methods 

/**
 *  @brief Call @c stop on a TCP acceptor @c net_entity and remove it from the internal
 *  list of TCP acceptors.
 *
 *  @param acc TCP acceptor @c net_entity to be stopped and removed.
 *
 */
  void remove(tcp_acceptor_net_entity acc) {
    acc.stop();
    std::experimental::net::post(m_ioc.get_executor(), 
          [acc, this] () mutable {
        chops::erase_where(m_acceptors, acc.get_ptr());
      }
    );
  }

/**
 *  @brief Call @c stop on a TCP connector @c net_entity and remove it from the internal
 *  list of TCP connectors.
 *
 *  @param conn TCP connector @c net_entity to be stopped and removed.
 *
 */
  void remove(tcp_connector_net_entity conn) {
    conn.stop();
    std::experimental::net::post(m_ioc.get_executor(), 
          [conn, this] () mutable {
        chops::erase_where(m_connectors, conn.get_ptr());
      }
    );
  }

/**
 *  @brief Call @c stop on a UDP @c net_entity and remove it from the internal
 *  list of UDP entities.
 *
 *  @param udp_ent UDP @c net_entity to be stopped and removed.
 *
 */
  void remove(udp_net_entity udp_ent) {
    udp_ent.stop();
    std::experimental::net::post(m_ioc.get_executor(), 
          [udp_ent, this] () mutable {
        chops::erase_where(m_udp_entities, udp_ent.get_ptr());
      }
    );
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

