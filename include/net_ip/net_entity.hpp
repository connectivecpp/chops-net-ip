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
#include <type_traits> // std::is_invocable
#include <system_error> // std::make_error, std::error_code

#include "nonstd/expected.hpp"

#include "net_ip/net_ip_error.hpp"

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/detail/wp_access.hpp"

#include "net_ip/io_type_decls.hpp"

#include "utility/overloaded.hpp"

namespace chops {
namespace net {

// Cliff note: when C++ 20 lambda templates are available much of this code can be simplified,
// since most of it is generic (just doesn't have the specific type parameter available as 
// needed in the right place). Stating it another way, there is waaaaaaay too much boilerplate 
// code (it may be possible to simplify with C++17 techniques that I don't know about yet).
  
/**
 *  @brief The @c net_entity class provides the application interface 
 *  into the TCP acceptor, TCP connector, and UDP entity functionality.
 *
 *  The @c net_entity class provides methods to start and stop processing 
 *  on an underlying network entity, such as a TCP acceptor or TCP connector or
 *  UDP entity (which may be a UDP unicast sender or receiver, or a UDP
 *  multicast receiver).
 *
 *  Calling the @c stop method on a @c net_entity object will shutdown the 
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
  using udp_wp = detail::udp_entity_io_weak_ptr;
  using acc_wp = detail::tcp_acceptor_weak_ptr;
  using conn_wp = detail::tcp_connector_weak_ptr;

private:
  std::variant<udp_wp, acc_wp, conn_wp> m_wptr;

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
  explicit net_entity (const std::shared_ptr<ET>& p) noexcept : m_wptr(p) { }

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
 *  @return @c nonstd::expected - @c bool on success, specifying whether @c start been 
 *  called (if @c false, the network entity has not been started or is in a stopped state); 
 *  on error (if no associated IO handler), a @c std::error_code is returned.
 */
  auto is_started() const -> 
          nonstd::expected<bool, std::error_code> {
    return std::visit(chops::overloaded {
        [] (const udp_wp& wp) -> nonstd::expected<bool, std::error_code> {
          return detail::wp_access<bool>(wp, 
                 [] (detail::udp_entity_io_shared_ptr sp) { return sp->is_started(); } );
        },
        [] (const acc_wp& wp) -> nonstd::expected<bool, std::error_code> {
          return detail::wp_access<bool>(wp,
                 [] (detail::tcp_acceptor_shared_ptr sp) { return sp->is_started(); } );
        },
        [] (const conn_wp& wp) -> nonstd::expected<bool, std::error_code> {
          return detail::wp_access<bool>(wp,
                 [] (detail::tcp_connector_shared_ptr sp) { return sp->is_started(); } );
        },
      },  m_wptr);
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
 *  @return @c nonstd::expected - socket has been visited on success; on error (if no 
 *  associated IO handler), a @c std::error_code is returned.
 */
  template <typename F>
  auto visit_socket(F&& func) const ->
          nonstd::expected<void, std::error_code> {
    return std::visit(chops::overloaded {
        [&func] (const udp_wp& wp) -> nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_v<F, asio::ip::udp::socket&>) {
            return detail::wp_access_void(wp,
                [&func] (detail::udp_entity_io_shared_ptr sp) { sp->visit_socket(func); return std::error_code(); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&func] (const acc_wp& wp) -> nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_v<F, asio::ip::tcp::acceptor&>) {
            return detail::wp_access_void(wp,
                [&func] (detail::tcp_acceptor_shared_ptr sp) { sp->visit_socket(func); return std::error_code(); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&func] (const conn_wp& wp) -> nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_v<F, asio::ip::tcp::socket&>) {
            return detail::wp_access_void(wp,
                [&func] (detail::tcp_connector_shared_ptr sp) { sp->visit_socket(func); return std::error_code(); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
      },  m_wptr);
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
 *    std::size_t (chops::net::tcp_io_output); // TCP
 *    std::size_t (chops::net::udp_io_output); // UDP
 *  @endcode
 *
 *  The function object will be called 0 to N times depending on active IO handlers. An
 *  IO handler is active is @c start_io has been called on it. The return value specifies
 *  how many times the function object has been called.
 *
 *  @return Number of times function object called, or a @c std::error_code is returned.
 *  @return @c nonstd::expected - on success returns number of times function object has
 *  been called; on error (if no associated IO handler), a @c std::error_code is returned.
 */
  template <typename F>
  auto visit_io_output(F&& func) const ->
          nonstd::expected<std::size_t, std::error_code> {
    return std::visit(chops::overloaded {
        [&func] (const udp_wp& wp)-> nonstd::expected<std::size_t, std::error_code>  {
          if constexpr (std::is_invocable_v<F, chops::net::udp_io_output>) {
            return detail::wp_access<std::size_t>(wp,
                [&func] (detail::udp_entity_io_shared_ptr sp) { return sp->visit_io_output(func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&func] (const acc_wp& wp)-> nonstd::expected<std::size_t, std::error_code>  {
          if constexpr (std::is_invocable_v<F, chops::net::tcp_io_output>) {
            return detail::wp_access<std::size_t>(wp,
                [&func] (detail::tcp_acceptor_shared_ptr sp) { return sp->visit_io_output(func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&func] (const conn_wp& wp)-> nonstd::expected<std::size_t, std::error_code>  {
          if constexpr (std::is_invocable_v<F, chops::net::tcp_io_output>) {
            return detail::wp_access<std::size_t>(wp,
                [&func] (detail::tcp_connector_shared_ptr sp) { return sp->visit_io_output(func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
      },  m_wptr);
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
 *  on the @c basic_io_interface object should be called, as the underlying handler, if
 *  present, is likely being destructed. The @c basic_io_interface object is provided
 *  as a key to associate multiple error codes to the same handler.
 *
 *  2) The error code associated with the invocation. There are error codes associated 
 *  with application initiated closes, shutdowns, and other state changes as well as error 
 *  codes for network or system errors.
 *
 *  The @c err_func callback may be invoked in contexts other than a network IO error - for
 *  example, if a TCP acceptor or UDP entity cannot bind to a local port, a system error 
 *  code will be provided. It is also used to notify important state changes, such as 
 *  a message handler shutdown or TCP connector state changes.
 *
 *  The error function object must be copyable (it will be stored in a @c std::function).
 *
 *  For use cases that don't care about error codes, a function named @c empty_error_func 
 *  is available.
 *
 *  @return @c nonstd::expected - on success network entity is started; on error, a 
 *  @c std::error_code is returned.
 */
  template <typename F1, typename F2>
  auto start(F1&& io_state_chg_func, F2&& err_func) ->
          nonstd::expected<void, std::error_code> {
    return std::visit(chops::overloaded {
        [&io_state_chg_func, &err_func] (const udp_wp& wp)->nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_r_v<bool, F1, udp_io_interface, std::size_t, bool> &&
                        std::is_invocable_v<F2, udp_io_interface, std::error_code>) {
            return detail::wp_access_void(wp,
                [&io_state_chg_func, &err_func] (detail::udp_entity_io_shared_ptr sp) 
                  { return sp->start(io_state_chg_func, err_func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&io_state_chg_func, &err_func] (const acc_wp& wp)->nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_r_v<bool, F1, tcp_io_interface, std::size_t, bool> &&
                        std::is_invocable_v<F2, tcp_io_interface, std::error_code>) {
            return detail::wp_access_void(wp,
                [&io_state_chg_func, &err_func] (detail::tcp_acceptor_shared_ptr sp) 
                  { return sp->start(io_state_chg_func, err_func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
        [&io_state_chg_func, &err_func] (const conn_wp& wp)->nonstd::expected<void, std::error_code> {
          if constexpr (std::is_invocable_r_v<bool, F1, tcp_io_interface, std::size_t, bool> &&
                        std::is_invocable_v<F2, tcp_io_interface, std::error_code>) {
            return detail::wp_access_void(wp,
                [&io_state_chg_func, &err_func] (detail::tcp_connector_shared_ptr sp) 
                  { return sp->start(io_state_chg_func, err_func); } );
          }
          return nonstd::make_unexpected(std::make_error_code(net_ip_errc::functor_variant_mismatch));
        },
      },  m_wptr);
  }

/**
 *  @brief Stop network processing on the associated network entity.
 *
 *  Internally, the network entity will call @c stop_io (or equivalent) on each associated
 *  IO handler.
 *
 *  Stopping the network processing may involve closing connections, deallocating
 *  resources, unbinding from ports, and invoking application provided state change function 
 *  object callbacks. 
 *
 *  @return @c nonstd::expected - on success network entity is stopped; on error, a 
 *  @c std::error_code is returned.
 */
  auto stop() ->
          nonstd::expected<void, std::error_code> {
    return std::visit(chops::overloaded {
        [] (const udp_wp& wp)->nonstd::expected<void, std::error_code> {
          return detail::wp_access_void(wp, 
              [] (detail::udp_entity_io_shared_ptr sp) { return sp->stop(); } );
        },
        [] (const acc_wp& wp)->nonstd::expected<void, std::error_code> {
          return detail::wp_access_void(wp, 
              [] (detail::tcp_acceptor_shared_ptr sp) { return sp->stop(); } );
        },
        [] (const conn_wp& wp)->nonstd::expected<void, std::error_code> {
          return detail::wp_access_void(wp, 
              [] (detail::tcp_connector_shared_ptr sp) { return sp->stop(); } );
        },
      },  m_wptr);
  }

  friend bool operator==(const net_entity&, const net_entity&) noexcept;
  friend bool operator<(const net_entity&, const net_entity&) noexcept;
};

/**
 *  @brief Compare two @c net_entity objects for equality.
 *
 *  If the @ net_entity objects are not both pointing to the same type of network entity 
 *  (TCP connector, TCP acceptor, etc), then they are not equal. If both are the same
 *  type of network entity, then both are checked to be valid (i.e. the internal @c weak_ptr
 *  is valid). If both are valid, then a @c std::shared_ptr comparison is made.
 *
 *  If both @c net_entity objects are invalid (and both same type of network entity), @c true 
 *  is returned (this implies that all invalid @c net_entity objects are equivalent). If one 
 *  is valid and the other invalid, @c false is returned.
 *
 *  @return As described in the comments.
 */

inline bool operator==(const net_entity& lhs, const net_entity& rhs) noexcept {
  return std::visit(chops::overloaded {
    [] (const net_entity::udp_wp& lwp, const net_entity::udp_wp& rwp) {
          return lwp.lock() == rwp.lock();
        },
    [] (const net_entity::udp_wp& lwp, const net_entity::acc_wp& rwp) {
          return false;
        },
    [] (const net_entity::udp_wp& lwp, const net_entity::conn_wp& rwp) {
          return false;
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::acc_wp& rwp) {
          return lwp.lock() == rwp.lock();
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::udp_wp& rwp) {
          return false;
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::conn_wp& rwp) {
          return false;
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::conn_wp& rwp) {
          return lwp.lock() == rwp.lock();
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::udp_wp& rwp) {
          return false;
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::acc_wp& rwp) {
          return false;
        },
    }, lhs.m_wptr, rhs.m_wptr);
}

/**
 *  @brief Compare two @c net_entity objects for ordering purposes.
 *
 *  Arbitrarily, a UDP network entity compares less than a TCP acceptor which compares
 *  less than a TCP connector. If both network entities are the same then the 
 *  @c std::shared_ptr ordering is returned.
 *
 *  All invalid @c net_entity objects (of the same network entity type) are less than valid 
 *  ones. If both @c net_entity objects are invalid and the same network entity type, 
 *  they are considered equal, so @c operator< returns @c false.
 *
 *  @return As described in the comments.
 */
inline bool operator<(const net_entity& lhs, const net_entity& rhs) noexcept {
  return std::visit(chops::overloaded {
    [] (const net_entity::udp_wp& lwp, const net_entity::udp_wp& rwp) {
          return lwp.lock() < rwp.lock();
        },
    [] (const net_entity::udp_wp& lwp, const net_entity::acc_wp& rwp) {
          return true;
        },
    [] (const net_entity::udp_wp& lwp, const net_entity::conn_wp& rwp) {
          return true;
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::acc_wp& rwp) {
          return lwp.lock() < rwp.lock();
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::udp_wp& rwp) {
          return false;
        },
    [] (const net_entity::acc_wp& lwp, const net_entity::conn_wp& rwp) {
          return true;
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::conn_wp& rwp) {
          return lwp.lock() < rwp.lock();
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::udp_wp& rwp) {
          return false;
        },
    [] (const net_entity::conn_wp& lwp, const net_entity::acc_wp& rwp) {
          return false;
        },
    }, lhs.m_wptr, rhs.m_wptr);
  }

/**
 *  @brief A "do nothing" error function template that can be used in the 
 *  @c net_entity @c start method.
 *
 *  @relates net_entity
 */
template <typename IOT>
void empty_error_func(basic_io_interface<IOT>, std::error_code) { } 

/**
 *  @brief A "do nothing" error function used in the @c net_entity @c start 
 *  method, for TCP @c basic_io_interface objects.
 *
 *  @relates net_entity
 */
inline void tcp_empty_error_func(tcp_io_interface, std::error_code) { }

/**
 *  @brief A "do nothing" error function used in the @c net_entity @c start 
 *  method, for UDP @c basic_io_interface objects.
 *
 *  @relates net_entity
 */
inline void udp_empty_error_func(udp_io_interface, std::error_code) { }


} // end net namespace
} // end chops namespace

#endif

