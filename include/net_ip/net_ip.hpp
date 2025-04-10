/** @file 
 *
 *  @defgroup net_ip_module Classes and functions for the Chops Net IP library.
 *
 *  @ingroup net_ip_module
 *
 *  @brief Chops @c net_ip networking class and related functions and facilities.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef NET_IP_HPP_INCLUDED
#define NET_IP_HPP_INCLUDED

#include <memory> // std::shared_ptr
#include <cstddef> // std::size_t
#include <string_view>
#include <vector>
#include <chrono>
#include <variant> // std::visit
#include <type_traits> // std::enable_if

#include <mutex> // std::scoped_lock, std::mutex

#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"

#include "net_ip/net_ip_error.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/tcp_connector_timeout.hpp"

#include "utility/erase_where.hpp"
#include "utility/overloaded.hpp"

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
 *  Applications perform operations with the @c net_entity and 
 *  @c basic_io_interface and @c basic_io_output objects. The @c net_ip object 
 *  creates facade-like objects of type @c net_entity, which allow further 
 *  operations.
 *
 *  The general application usage pattern for the @ net_ip, @ net_entity, 
 *  @c basic_io_interface and @c basic_io_output classes is:
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
 *  using direct (synchronous) lookups when the @c net_entity @c start method
 *  is called. Remote host and port name lookups are performed asynchronously 
 *  (since these may take longer) and are only needed for TCP connectors.
 *  If this is not acceptable, the application can perform the lookup and the 
 *  endpoint (or endpoint sequence) can be passed in through the @c make method.
 *
 *  State change function objects are invoked when network IO can be started as
 *  well as when an error or shutdown occurs.
 *
 *  4. When a @c basic_io_interface object is supplied to the application through
 *  the IO state change callback, input processing is started through a 
 *  @c start_io call. For outbound data, a @c basic_io_output object can be created
 *  from the @c basic_io_interface object, allowing data to be sent through @c send 
 *  methods.
 *
 *  There are no executor operations available through the @c net_ip class. In 
 *  other words, no event loop or @c run methods are available. Instead, the
 *  @c net_ip class takes an @c asio @c io_context as a constructor parameter and 
 *  application code will use the @c asio executor methods for invoking
 *  the underlying asynchronous operations.
 *
 *  For convenience, a class named @c worker in the @c net_ip_component directory 
 *  combines an executor with a work guard and creates a thread to invoke the asynchronous 
 *  operations. Example usage:
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
 *
 *  It should be noted, however, that race conditions are possible, especially for 
 *  similar operations invoked between @c net_entity and @c basic_io_interface 
 *  objects. For example, starting and stopping network entities concurrently between 
 *  separate objects or threads could cause unexpected behavior.
 *
 */
class net_ip {
private:

  asio::io_context&                             m_ioc;
  mutable std::mutex                            m_mutex;

  std::vector<detail::tcp_acceptor_shared_ptr>  m_acceptors;
  std::vector<detail::tcp_connector_shared_ptr> m_connectors;
  std::vector<detail::udp_entity_io_shared_ptr> m_udp_entities;

private:
  using lg = std::scoped_lock<std::mutex>;

public:

/**
 *  @brief Construct a @c net_ip object without starting any network processing.
 *
 *  @param ioc IO context for asynchronous operations.
 */
  explicit net_ip(asio::io_context& ioc) :
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
 *  The port (and optional listen interface) passed in to this constructor will be
 *  resolved (name lookup) when @c start is called on the @c net_entity.
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
 *  @return @c net_entity object instantiated for a TCP acceptor.
 *
 */
  net_entity make_tcp_acceptor (std::string_view local_port_or_service, 
                                std::string_view listen_intf = "",
                                bool reuse_addr = true) {
    auto p = std::make_shared<detail::tcp_acceptor>(m_ioc, local_port_or_service, 
                                                    listen_intf, reuse_addr);
    lg g(m_mutex);
    m_acceptors.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a TCP acceptor @c net_entity, using an already created endpoint.
 *
 *  This @c make method allows flexibility in creating an endpoint for the acceptor to use, 
 *  such as directly specifying ipV4 or ipV6 in name resolving, or directly creating the endpoint 
 *  without using name resolving.
 *
 *  @param endp A @c asio::ip::tcp::endpoint that the acceptor uses for the local
 *  bind (when @c start is called).
 *
 *  @param reuse_addr If @c true (default), the @c reuse_address socket option is set upon 
 *  socket open.
 *
 *  @return @c net_entity object instantiated for a TCP acceptor.
 *
 */
  net_entity make_tcp_acceptor (const asio::ip::tcp::endpoint& endp,
                                bool reuse_addr = true) {
    auto p = std::make_shared<detail::tcp_acceptor>(m_ioc, endp, reuse_addr);
    lg g(m_mutex);
    m_acceptors.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity, which will perform an active TCP
 *  connect to the specified host and port (once started).
 *
 *  Internally a sequence of remote endpoints will be looked up through a name resolver,
 *  (after @c start on the @c net_entity is called), and each endpoint will be tried in 
 *  succession.
 *
 *  @param remote_port_or_service Port number or service name on remote host.
 *
 *  @param remote_host Remote host name or IP address.
 *
 *  @param timeout_func @c tcp_connector_timeout_func, which must be copyable, returns a 
 *  timeout value for retries on connect failures.
 *
 *  @param reconn_on_err When a TCP connection has been established and a network error occurs,
 *  this flag specifies whether to start a reconnect attempt; this allows connectors that
 *  run until explicitly stopped.
 *
 *  @return @c net_entity object instantiated for a TCP connector.
 *
 *  @note The name and port lookup to create a sequence of remote TCP endpoints is performed
 *  when the @c net_entity @c start method is called. If this is not acceptable, the
 *  endpoints can be looked up by the application and the alternate @c make_tcp_connector
 *  method called.
 *
 */
  template <typename F = simple_timeout>
  net_entity make_tcp_connector (std::string_view remote_port_or_service,
                                 std::string_view remote_host,
                                 const F& timeout_func = simple_timeout { },
                                 bool reconn_on_err = false) {

    auto p = std::make_shared<detail::tcp_connector>(m_ioc, remote_port_or_service, remote_host, 
                                                     tcp_connector_timeout_func(timeout_func),
                                                     reconn_on_err);
    lg g(m_mutex);
    m_connectors.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity with the endpoints already created, passing
 *  in iterators to the endpoint container.
 *
 *  This method allows flexibility in creating the remote endpoints for the connector to use.
 *  It also bypasses the name lookups (DNS lookups) that happen when a remote host and port
 *  is used.
 *
 *  @param beg A begin iterator to a sequence of remote @c asio::ip::tcp::endpoint
 *  objects.
 *
 *  @param end An end iterator to the sequence of endpoints.
 *
 *  @param timeout_func @c tcp_connector_timeout_func, which must be copyable, returns a 
 *  timeout value for retries on connect failures.
 *
 *  @param reconn_on_err When a TCP connection has been established and a network error occurs,
 *  this flag specifies whether to start a reconnect attempt; this allows connectors that
 *  run until explicitly stopped.
 *
 *  @return @c net_entity object instantiated for a TCP connector.
 *
 *  @note To prevent selection of this method when two @c const @c char* parameters are 
 *  provided for the first two parameters, this method is conditionally enabled. @c const
 *  @c char* or @c char* should match the @c make_tcp_connector method taking 
 *  @c std::string_view parameters, not this method.
 */
  template <typename Iter, typename F = simple_timeout>
  auto make_tcp_connector (Iter beg, Iter end,
                           const F& timeout_func = simple_timeout { },
                           bool reconn_on_err = false) ->
        std::enable_if_t<std::is_same_v<std::decay<decltype(*beg)>, asio::ip::tcp::endpoint>, net_entity> {
    auto p = std::make_shared<detail::tcp_connector>(m_ioc, beg, end, 
                                                     tcp_connector_timeout_func(timeout_func),
                                                     reconn_on_err);
    lg g(m_mutex);
    m_connectors.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a TCP connector @c net_entity using a single remote endpoint.
 *
 *  @param endp Remote @c asio::ip::tcp::endpoint to use for the connect
 *  attempt.
 *
 *  @param timeout_func @c tcp_connector_timeout_func, which must be copyable, returns a 
 *  timeout value for retries on connect failures.
 *
 *  @param reconn_on_err When a TCP connection has been established and a network error occurs,
 *  this flag specifies whether to start a reconnect attempt; this allows connectors that
 *  run until explicitly stopped.
 *
 *  @return @c net_entity object instantiated for a TCP connector.
 *
 */
  template <typename F = simple_timeout>
  net_entity make_tcp_connector (const asio::ip::tcp::endpoint& endp,
                                 const F& timeout_func = simple_timeout { },
                                 bool reconn_on_err = false) {
    std::vector<asio::ip::tcp::endpoint> vec { endp };
    return make_tcp_connector(vec.cbegin(), vec.cend(), timeout_func, reconn_on_err);
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
 *  Names are resolved and a bind to the local endpoint started when the 
 *  @c net_entity @c start method is called, and a read is not started until the 
 *  @c io_interface @c start_io method is called.
 *
 *  @param local_port_or_service Port number or service name for local binding.
 *
 *  @param local_intf Local interface name, otherwise the default is "any address".
 *
 *  @return @c net_entity object instantiated for UDP.
 *
 *  @note Common socket options on UDP datagram sockets, such as increasing the 
 *  "time to live" (hop limit), allowing UDP broadcast, or setting the socket 
 *  reuse flag can be set by using the @c net_entity @c visit_socket method (or 
 *  @c basic_io_interface @c visit_socket method, which returns the same reference).
 *
 */
  net_entity make_udp_unicast (std::string_view local_port_or_service, 
                               std::string_view local_intf = "") {
    auto p = std::make_shared<detail::udp_entity_io>(m_ioc, local_port_or_service, local_intf);
    lg g(m_mutex);
    m_udp_entities.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a UDP unicast @c net_entity for receiving and sending, using an already 
 *  created endpoint.
 *
 *  This @c make method allows flexibility in creating an endpoint for the UDP unicast
 *  @c net_entity to use.
 *
 *  @param endp A @c asio::ip::udp::endpoint used for the local bind 
 *  (when @c start is called).
 *
 *  @return @c net_entity object instantiated for UDP.
 *
 */
  net_entity make_udp_unicast (const asio::ip::udp::endpoint& endp) {
    auto p = std::make_shared<detail::udp_entity_io>(m_ioc, endp);
    lg g(m_mutex);
    m_udp_entities.push_back(p);
    return net_entity(p);
  }

/**
 *  @brief Create a UDP unicast @c net_entity for sending only (no local bind is performed).
 *
 *  This @c make method is used when no UDP reads are desired, only sends.
 *
 *  @return @c net_entity object instantiated for UDP.
 *
 */
  net_entity make_udp_sender () {
    return make_udp_unicast(asio::ip::udp::endpoint());
  }

// TODO: multicast make methods 

/**
 *  @brief Remove a @c net_entity from the internal list of @c net_entity objects.
 *
 *  @c stop should first be called by the application, or the @c stop_all 
 *  method can be called to stop all net entities.
 *
 *  @param ent @c net_entity to be removed.
 *
 */
  void remove(net_entity ent) {
    lg g(m_mutex);
    std::visit (chops::overloaded {
        [this] (detail::tcp_acceptor_weak_ptr p) { chops::erase_where(m_acceptors, p.lock()); },
        [this] (detail::tcp_connector_weak_ptr p) { chops::erase_where(m_connectors, p.lock()); },
        [this] (detail::udp_entity_io_weak_ptr p) { chops::erase_where(m_udp_entities, p.lock()); },
      }, ent.m_wptr);
  }

/**
 *  @brief Remove all acceptors, connectors, and UDP entities.
 *
 *  @c stop_all (or the equivalent) should first be called to
 *  stop all net entities.
 *
 */
  void remove_all() {
    lg g(m_mutex);
    m_udp_entities.clear();
    m_connectors.clear();
    m_acceptors.clear();
  }

/**
 *  @brief Call @c stop on all acceptors, connectors, and UDP entities.
 *
 *  This method allows for a more measured shutdown, if needed.
 *
 */
  void stop_all() {
    lg g(m_mutex);
    for (auto i : m_udp_entities) { i->stop(); }
    for (auto i : m_connectors) { i->stop(); }
    for (auto i : m_acceptors) { i->stop(); }
  }

};

}  // end net namespace
}  // end chops namespace

#endif

