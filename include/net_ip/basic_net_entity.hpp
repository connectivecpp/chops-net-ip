/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief @c basic_net_entity class template.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef BASIC_NET_ENTITY_HPP_INCLUDED
#define BASIC_NET_ENTITY_HPP_INCLUDED


#include <memory> // std::shared_ptr, std::weak_ptr
#include <cstddef> // std::size_t 
#include <utility> // std::move, std::forward
#include <system_error> // std::make_error

#include "net_ip/net_ip_error.hpp"

namespace chops {
namespace net {

/**
 *  @brief The @c basic_net_entity class template provides the application interface 
 *  into the TCP acceptor, TCP connector, and UDP entity functionality.
 *
 *  The @c basic_net_entity class template provides methods to start and stop processing 
 *  on an underlying network entity, such as a TCP acceptor or TCP connector or
 *  UDP entity (which may be a UDP unicast sender or receiver, or a UDP
 *  multicast receiver).
 *
 *  Calling the @c stop method on an @c basic_net_entity object will shutdown the 
 *  associated network resource. At this point, other @c basic_net_entity objects 
 *  copied from the original will be affected.
 *
 *  The @c basic_net_entity class is a lightweight value class, designed to be easy 
 *  and efficient to copy and store. Internally it uses a @c std::weak_ptr to 
 *  refer to the actual network entity. 
 * 
 *  A @c basic_net_entity object is either associated with a network entity 
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present.
 *
 *  Applications can default construct a @c basic_net_entity object, but it is not 
 *  useful until a valid @c basic_net_entity object is assigned to it (as provided in the 
 *  @c make methods of the @c net_ip class).
 *
 *  Appropriate comparison operators are provided to allow @c basic_net_entity objects
 *  to be used in associative or sequence containers.
 *
 *  All @c basic_net_entity methods are safe to call concurrently from multiple
 *  threads, although it is questionable logic for multiple threads to call
 *  @c start or @c stop at the same time.
 *
 */

template <typename ET>
class basic_net_entity {
private:
  std::weak_ptr<ET> m_eh_wptr;

public:

/**
 *  @brief Default construct a @c basic_net_entity object.
 *
 *  A @c basic_net_entity object is not useful until an active @c basic_net_entity is assigned 
 *  into it.
 *  
 */
  basic_net_entity () = default;

  basic_net_entity(const basic_net_entity&) = default;
  basic_net_entity(basic_net_entity&&) = default;

  basic_net_entity<ET>& operator=(const basic_net_entity&) = default;
  basic_net_entity<ET>& operator=(basic_net_entity&&) = default;

/**
 *  @brief Construct with a shared weak pointer to an internal net entity, this is an
 *  internal constructor only and not to be used by application code.
 */

  explicit basic_net_entity (std::weak_ptr<ET> p) noexcept : m_eh_wptr(p) { }

/**
 *  @brief Query whether an internal net entity is associated with this object.
 *
 *  If @c true, a net entity (TCP acceptor or connector or UDP entity) is associated. 
 *
 *  @return @c true if associated with a net entity.
 */
  bool is_valid() const noexcept { return !m_eh_wptr.expired(); }

/**
 *  @brief Query whether @c start has been called or not.
 *
 *  @return @c true if @c start has been called, @c false otherwise.
 *
 *  @throw A @c net_ip_exception is thrown if no association to a net entity.
 */
  bool is_started() const {
    if (auto p = m_eh_wptr.lock()) {
      return p->is_started();
    }
    throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
  }

/**
 *  @brief Return a reference to the underlying net entity socket, allowing socket 
 *  options to be queried or set or other socket methods to be called.
 *
 *  The socket reference returned from this method allows direct access to the
 *  @c basic_net_entity socket. This socket may be different from the socket that is
 *  accessible hrough the @c basic_io_interface object. In particular, a TCP acceptor
 *  socket reference is of type @c ip::tcp::acceptor (in the namespace
 *  @c std::experimental::net), a TCP connector socket reference is of type
 *  @c ip::tcp::socket, and a UDP entity socket reference is of type 
 *  @c ip::udp::socket.
 *
 *  @return @c ip::tcp::socket or @c ip::udp::socket or @c ip::tcp::acceptor, 
 *  depending on net entity type.
 *
 *  @throw A @c net_ip_exception is thrown if there is not an associated net entity.
 */
  typename ET::socket_type& get_socket() const {
    if (auto p = m_eh_wptr.lock()) {
      return p->get_socket();
    }
    throw net_ip_exception(std::make_error_code(net_ip_errc::weak_ptr_expired));
  }

/**
 *  @brief Start network processing on the associated net entity with the application
 *  providing state change and error function object callbacks.
 *
 *  Once a net entity (TCP acceptor, TCP connector, UDP entity) is created through 
 *  a @c net_ip @c make method, calling @c start on the @c basic_net_entity causes local port 
 *  binding and other processing (e.g. TCP listen, TCP connect) to occur.
 *
 *  Input and output processing does not start until the @c io_interface @c start_io
 *  method is called.
 *
 *  The application provides two function objects to @c start:
 *
 *  1) A state change function object which is invoked twice. The first time is when a 
 *  TCP connection is created or UDP socket is opened, the second time is when the TCP 
 *  connection is destroyed or the UDP socket closed. An @c io_interface object is 
 *  provided to the callback which allows IO processing to commence through the 
 *  @c start_io method call.
 *
 *  2) An error function object which is invoked whenever an error occurs or when
 *  processing is gracefully shutdown.
 *
 *  The @c start method call can be followed by a @c stop call, followed by @c start, 
 *  etc. This may be useful (for example) for a TCP connector that needs to reconnect 
 *  after a connection has been lost.
 *
 *  @param state_chg_func A function object with the following signature:
 *
 *  @code
 *    // TCP:
 *    void (chops::net::tcp_io_interface, std::size_t, bool);
 *    // UDP:
 *    void (chops::net::udp_io_interface, std::size_t, bool);
 *  @endcode
 *
 *  The parameters are as follows:
 *
 *  1) An @c io_interface object providing @c start_io access (and @c stop_io access if
 *  needed) to the underlying IO handler.
 *
 *  2) A count of the underlying IO handlers associated with this net entity. For 
 *  a TCP connector or a UDP entity the number is 1, and for a TCP acceptor the number is 
 *  1 to N, depending on the number of accepted connections.
 *
 *  3) If @c true, the @c io_interface has just been created (i.e. a TCP connection 
 *  has been created or a UDP socket is ready), and if @c false, the connection or socket
 *  has been destroyed or closed.
 *
 *  In both invocations the @c io_interface object is valid (@c is_valid returns @c true).
 *  For the second invocation no @c io_interface methods should be called, but the 
 *  @c io_interface object can be used for associative lookups (if needed).
 *
 *  The state change function object must be copyable (it will be stored in a @c std::function).
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
 *  1) An @c io_interface object, which may or may not be valid (i.e @c is_valid may
 *  return either @c true or @c false), depending on the context of the error. No methods 
 *  on the @c io_interface object should be called, as the underlying handler is being 
 *  destructed.
 *
 *  2) The error code associated with the shutdown. There are error codes associated 
 *  with graceful shutdown as well as error codes for network or system errors.
 *
 *  The @c err_func callback may be invoked in contexts other than an IO error - for
 *  example, if a TCP acceptor or UDP entity cannot bind to a local port, a system error 
 *  code will be provided.
 *
 *  The error function object must be copyable (it will be stored in a @c std::function).
 *
 *  @return @c true if valid net entity association.
 *
 */
  template <typename F1, typename F2>
  bool start(F1&& state_chg_func, F2&& err_func) {
    auto p = m_eh_wptr.lock();
    return p ? (p->start(std::forward<F1>(state_chg_func), std::forward<F2>(err_func)), true) : false;
  }

/**
 *  @brief Start network processing on the associated net entity where only an
 *  state change function object is provided and error callbacks are not 
 *  desired.
 *
 *  @param state_chg_func A function object as described in the other @c start 
 *  method.
 *
 *  @return @c true if valid net entity association.
 */
  template <typename F>
  bool start(F&& state_chg_func) {
    auto p = m_eh_wptr.lock();
    return p ? (p->start(std::forward<F>(state_chg_func)), true) : false;
  }

/**
 *  @brief Stop network processing on the associated net entity after calling @c stop_io on
 *  each associated IO handler.
 *
 *  Stopping the network processing may involve closing connections, deallocating
 *  resources, unbinding from ports, and invoking application provided state change function 
 *  object callbacks. 
 *
 *  @return @c true if valid net entity association.
 *
 */
  bool stop() {
    auto p = m_eh_wptr.lock();
    return p ? (p->stop(), true) : false;
  }
/**
 *  @brief Compare two @c basic_net_entity objects for equality.
 *
 *  If both @c basic_net_entity objects are valid, then a @c std::shared_ptr comparison is made.
 *  If both @c basic_net_entity objects are invalid, @c true is returned (this implies that
 *  all invalid @c basic_net_entity objects are equivalent). If one is valid and the other invalid,
 *  @c false is returned.
 *
 *  @return As described in the comments.
 */

  bool operator==(const basic_net_entity<ET>& rhs) const noexcept {
    auto lp = m_eh_wptr.lock();
    auto rp = rhs.m_eh_wptr.lock();
    return (lp && rp && lp == rp) || (!lp && !rp);
  }

/**
 *  @brief Compare two @c basic_net_entity objects for ordering purposes.
 *
 *  All invalid @c basic_net_entity objects are less than valid ones. When both are valid,
 *  the @c std::shared_ptr ordering is returned.
 *
 *  Specifically, if both @c basic_net_entity objects are invalid @c false is returned. 
 *  If the left @c basic_net_entity object is invalid and the right is not, @c true is
 *  returned. If the right @c basic_net_entity is invalid, but the left is not, @c false is 
 *  returned.
 *
 *  @return As described in the comments.
 */
  bool operator<(const basic_net_entity<ET>& rhs) const noexcept {
    auto lp = m_eh_wptr.lock();
    auto rp = rhs.m_eh_wptr.lock();
    return (lp && rp && lp < rp) || (!lp && rp);
  }

/**
 *  @brief Return a @c shared_ptr to the actual net entity object, meant to be used
 *  for internal purposes only.
 *
 *  @return As described in the comments.
 */
  auto get_ptr() const noexcept {
    return m_eh_wptr.lock();
  }

};

} // end net namespace
} // end chops namespace

#endif

