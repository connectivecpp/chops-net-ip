/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c net_entity class and related functionality.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef NET_ENTITY_HPP_INCLUDED
#define NET_ENTITY_HPP_INCLUDED


#include <memory> // std::shared_ptr, std::weak_ptr
#include <cstddef> // std::size_t 
#include <utility> // std::move, std::forward
#include <variant>
#include <system_error> // std::make_error, std::error_code

#include "net_ip/net_ip_error.hpp"
#include "net_ip/basic_io_interface.hpp"

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c net_entity class provides the application interface 
 *  into the TCP acceptor, TCP connector, and UDP entity functionality.
 *
 *  The @c net_entity class provides methods to start and stop processing 
 *  on an underlying network entity, such as a TCP acceptor or TCP connector or
 *  UDP entity (which may be a UDP unicast sender or receiver, or a UDP
 *  multicast receiver).
 *
 *  Calling the @c stop method on an @c net_entity object will shutdown the 
 *  associated network resource. At this point, other @c net_entity objects 
 *  copied from the original will be affected.
 *
 *  The @c net_entity class is a lightweight value class, designed to be easy 
 *  and efficient to copy and store. Internally it uses a @c std::weak_ptr to 
 *  refer to the actual network entity. 
 * 
 *  A @c net_entity object is either associated with a network entity 
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present.
 *
 *  Applications can default construct a @c net_entity object, but it is not 
 *  useful until a valid @c net_entity object is assigned to it (as provided in the 
 *  @c make methods of the @c net_ip class).
 *
 *  Appropriate comparison operators are provided to allow @c net_entity objects
 *  to be used in associative or sequence containers.
 *
 *  All @c net_entity methods are safe to call concurrently from multiple
 *  threads, although it is questionable logic for multiple threads to call
 *  @c start or @c stop at the same time.
 *
 */

class net_entity {
private:
  std::variant<detail::udp_entity_io_weak_ptr,
               detail::tcp_acceptor_weak_ptr,
               detail::tcp_connector_weak_ptr> m_wptr;

private:
  friend class net_ip;

public:

/**
 *  @brief Default construct a @c net_entity object.
 *
 *  A @c net_entity object is not useful until an active @c net_entity is assigned 
 *  into it.
 *  
 */
  net_entity () = default;

  net_entity(const net_entity&) = default;
  net_entity(net_entity&&) = default;

  net_entity& operator=(const net_entity&) = default;
  net_entity& operator=(net_entity&&) = default;

/**
 *  @brief Construct with a shared weak pointer to an internal net entity, this is an
 *  internal constructor only and not to be used by application code.
 */

  template <typename ET>
  explicit net_entity (std::weak_ptr<ET> p) noexcept : m_wptr(p) { }

/**
 *  @brief Query whether an internal net entity is associated with this object.
 *
 *  If @c true, a net entity (TCP acceptor or TCP connector or UDP entity) is associated. 
 *
 *  @return @c true if associated with a net entity.
 */
  bool is_valid() const noexcept {
    return std::visit([] (const auto& wp) { return !wp.expired(); }, m_wptr);
  }

/**
 *  @brief Query whether the associated net entity is in a started or stopped state.
 *
 *  @return @c true if @c start has been called, @c false if not, or entity has
 *  stopped.
 *
 *  @throw A @c net_ip_exception is thrown if no association to a net entity.
 */
  bool is_started() const {
    return std::visit([] (const auto& wp)->bool { 
          if (auto p = wp.lock()) {
            return p->is_started();
          }
          throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
        }, m_wptr);
  }

/**
 *  @brief Call an application supplied function object with a reference to the associated 
 *  net entity @c asio socket.
 *
 *  The function object must have one of the following signatures, depending on the entity
 *  type:
 *
 *  @code
 *    void (asio::ip::tcp::socket&); // TCP connector
 *    void (asio::ip::tcp::acceptor&) // TCP acceptor
 *    void (asio::ip::udp::socket&); // UDP entity
 *  @endcode
 *
 *  Within the function object socket options can be queried or modified or any valid method
 *  called.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated net entity.
 */
  template <typename F>
  void visit_socket(F&& f) const {
    std::visit([func = std::forward<F>(f)] (const auto& wp) { 
        if (auto p = wp.lock()) {
          p->visit_socket(func);
        }
        throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
      }, m_wptr);
  }

/**
 *  @brief Call an application supplied function object with all @c basic_io_output objects
 *  that are active on associated IO handlers for this net entity.
 *
 *  A TCP connector will have 0 or 1 active IO handlers, depending on connection state, while
 *  a TCP acceptor will have 0 to N active IO handlers, depending on the number of incoming
 *  connections. A UDP entity will either have 0 or 1 active IO handlers depending on whether
 *  it has been started or not.
 *
 *  The function object must have one of the following signatures, depending on TCP or UDP:
 *
 *  @code
 *    void (chops::tcp_io_output); // TCP
 *    void (chops::udp_io_output); // UDP
 *  @endcode
 *
 *  The function object will be called 0 to N times depending on active IO handlers.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated net entity.
 */
  template <typename F>
  void visit_io_output(F&& f) const {
    std::visit([func = std::forward<F>(f)] (const auto& wp) { 
        if (auto p = wp.lock()) {
          p->visit_io_output(func);
        }
        throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
      }, m_wptr);
  }

/**
 *  @brief Start network processing on the associated net entity with the application
 *  providing IO state change and error function objects.
 *
 *  Once a net entity (TCP acceptor, TCP connector, UDP entity) is created through 
 *  a @c net_ip @c make method, calling @c start on the @c net_entity causes local port 
 *  binding and other processing (e.g. TCP listen, TCP connect) to occur.
 *
 *  Input and output processing does not start until the @c basic_io_interface @c start_io
 *  method is called.
 *
 *  The application provides two function objects to @c start (the second can be defaulted
 *  to a "do nothing" function object):
 *
 *  1) An IO state change callback function object. This callback object is invoked when a 
 *  TCP connection is created or a UDP socket opened, and then invoked when the TCP connection 
 *  is destroyed or the UDP socket closed. A @c basic_io_interface object is provided to the 
 *  callback which allows IO processing to commence through the @c start_io method call (when
 *  a TCP connection is created or UDP socket opened).
 *
 *  2) An error function object which is invoked whenever an error occurs or when
 *  processing is gracefully shutdown.
 *
 *  The @c start method call can be followed by a @c stop call, followed by @c start, 
 *  as needed.
 *
 *  @param io_state_chg_func A function object with the following signature:
 *
 *  @code
 *    // TCP:
 *    bool (chops::net::tcp_io_interface, std::size_t, bool);
 *    // UDP:
 *    bool (chops::net::udp_io_interface, std::size_t, bool);
 *  @endcode
 *
 *  The parameters are as follows:
 *
 *  1) A @c basic_io_interface object providing @c start_io access (and @c stop_io access if
 *  needed) to the underlying IO handler.
 *
 *  2) A count of the underlying IO handlers associated with this net entity. For 
 *  a TCP connector or a UDP entity the number is 1 when starting and 0 when stopping, and for 
 *  a TCP acceptor the number is 0 to N, depending on the number of accepted connections.
 *
 *  3) If @c true, the @c basic_io_interface has just been created (i.e. a TCP connection 
 *  has been created or a UDP socket is ready), and if @c false, the connection or socket
 *  has been destroyed or closed.
 *
 *  The return value specifies whether the net entity should continue processing or not.
 *  Returning @c false is the same as calling @c stop on the entity. Returning @c true means
 *  continue as expected.
 *
 *  Use cases for returning @c false include a TCP connector that should quit attempting to
 *  connect, or a TCP acceptor that might need to be shut down when all current TCP connections
 *  have closed.
 *
 *  In both IO state change function object callback invocations the @c basic_io_interface object 
 *  is valid (@c is_valid returns @c true). For the second invocation no @c basic_io_interface 
 *  methods should be called, but the @c basic_io_interface object can be used for associative 
 *  lookups (if needed).
 *
 *  The IO state change function object must be copyable (it will be stored in a 
 *  @c std::function).
 *
 *  @param err_func A function object with the following signature:
 *
 *  @code
 *    // TCP:
 *    void (chops::net::tcp_io_interface, std::error_code);
 *    // UDP:
 *    void (chops::net::udp_io_interface, std::error_code);
 *  @endcode
 *
 *  The parameters are as follows:
 *
 *  1) A @c basic_io_interface object, which may or may not be valid (i.e @c is_valid may
 *  return either @c true or @c false), depending on the context of the error. No methods 
 *  on the @c basic_io_interface object should be called, as the underlying handler is being 
 *  destructed.
 *
 *  2) The error code associated with the shutdown. There are error codes associated 
 *  with application initiated closes and graceful shutdown as well as error codes for 
 *  network or system errors.
 *
 *  The @c err_func callback may be invoked in contexts other than a network IO error - for
 *  example, if a TCP acceptor or UDP entity cannot bind to a local port, a system error 
 *  code will be provided.
 *
 *  The error function object must be copyable (it will be stored in a @c std::function).
 *
 *  For use cases that don't care about error codes, a function named @c empty_error_func 
 *  is available in the @c error_delivery.hpp header in the @c net_ip_component directory.
 *
 *  @return @c false if already started, otherwise @c true.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated net entity.
 */
  template <typename F1, typename F2>
  bool start(F1&& io_state_chg_func, F2&& err_func) {
    return std::visit([&io_state_chg_func, &err_func] (const auto& wp)->bool {
          if (auto p = wp.lock()) {
            return p->start(io_state_chg_func, err_func);
          }
          throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
        }, m_wptr);
  }

/**
 *  @brief Stop network processing on the associated net entity after calling @c stop_io on
 *  each associated IO handler.
 *
 *  Stopping the network processing may involve closing connections, deallocating
 *  resources, unbinding from ports, and invoking application provided state change function 
 *  object callbacks. 
 *
 *  @return @c false if already stopped, otherwise @c true.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated net entity.
 *
 */
  bool stop() {
    return std::visit([] (const auto& wp)->bool {
          if (auto p = wp.lock()) {
            return p->stop();
          }
          throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
        }, m_wptr);
  }
/**
 *  @brief Compare two @c net_entity objects for equality.
 *
 *  If both @c net_entity objects are valid, then a @c std::shared_ptr comparison is made.
 *  If both @c net_entity objects are invalid, @c true is returned (this implies that
 *  all invalid @c net_entity objects are equivalent). If one is valid and the other invalid,
 *  @c false is returned.
 *
 *  @return As described in the comments.
 */

  bool operator==(const net_entity& rhs) const noexcept {
    return std::visit([] (const auto& lwp, const auto& rwp) {
          auto lp = lwp.lock();
          auto rp = rwp.lock();
          return (lp && rp && lp == rp) || (!lp && !rp);
        }, m_wptr, rhs.m_wptr);
  }

/**
 *  @brief Compare two @c net_entity objects for ordering purposes.
 *
 *  All invalid @c net_entity objects are less than valid ones. When both are valid,
 *  the @c std::shared_ptr ordering is returned.
 *
 *  Specifically, if both @c net_entity objects are invalid @c false is returned. 
 *  If the left @c net_entity object is invalid and the right is not, @c true is
 *  returned. If the right @c net_entity is invalid, but the left is not, @c false is 
 *  returned.
 *
 *  @return As described in the comments.
 */
  bool operator<(const net_entity& rhs) const noexcept {
    return std::visit([] (const auto& lwp, const auto& rwp) {
          auto lp = lwp.lock();
          auto rp = rwp.lock();
          return (lp && rp && lp < rp) || (!lp && rp);
        }, m_wptr, rhs.m_wptr);
  }

};

} // end net namespace
} // end chops namespace

#endif

