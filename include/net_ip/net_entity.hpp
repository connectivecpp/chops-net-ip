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


#include <memory> // std::shared_ptr, std::weak_ptr
#include <cstddef> // std::size_t 
#include <utility> // std::forward
#include <system_error> // std::make_error

namespace chops {
namespace net {

/**
 *  @brief The @c net_entity class provides the application interface 
 *  into the TCP acceptor, TCP connector, and UDP entity functionality.
 *
 *  The @c net_entity class provides methods to start and stop processing on
 *  an underlying network entity, such as a TCP acceptor or TCP connector or
 *  UDP entity (which may be a UDP unicast sender or receiver, or a UDP
 *  multicast receiver).
 *
 *  The @c net_entity class is a lightweight value class, designed
 *  to be easy and efficient to copy and store. Internally it uses a 
 *  @c std::weak_ptr to refer to the actual network entity. 
 * 
 *  A @c net_entity object is either associated with a network entity 
 *  (i.e. the @c std::weak pointer is good), or not. The @c is_valid method queries 
 *  if the association is present.
 *
 *  Applications can default construct a @c net_entity object, but it is not useful 
 *  until a valid @c net_entity object is assigned to it (as provided in the make
 *  methods of the @c net_ip class).
 *
 *  Appropriate comparison operators are provided to allow @c net_entity objects
 *  to be used in associative or sequence containers.
 *
 *  All @c net_entity methods are safe to call concurrently from multiple
 *  threads, although it is questionable logic for multiple threads to call
 *  @c start or @c stop at the same time.
 *
 */

template <typename ET>
class net_entity {
private:
  std::weak_ptr<ET> m_eh_wptr;

public:

/**
 *  @brief Default construct a @c net_entity object.
 *
 *  A @c net_entity object is not useful until an active @c net_entity is assigned into it.
 *  
 */
  net_entity () = default;

  net_entity(const net_entity&) = default;
  net_entity(net_entity&&) = default;

  net_entity<ET>& operator=(const net_entity&) = default;
  net_entity<ET>& operator=(net_entity&&) = default;

/**
 *  @brief Construct with a shared weak pointer to an internal net entity, this is an
 *  internal constructor only and not to be used by application code.
 *
 */

  explicit net_entity (std::weak_ptr<ET> p) noexcept : m_eh_wptr(p) { }

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
 *  @brief Start network processing on the associated net entity with the application
 *  providing a state change function object callback.
 *
 *  Once a net entity (TCP acceptor, TCP connector, UDP entity) is created through 
 *  a @c net_ip @c make method, @c start is then called, and this is when
 *  local port binding and other processing (e.g. TCP listen, TCP connect) occurs.
 *
 *  Input and output processing does not start until the @c io_interface @c start_io
 *  method is called.
 *
 *  The application provides a state change function object callback which
 *  allows the internal net entity to notify the application when one of the 
 *  following occurs: 1) a TCP connect succeeds, or 2) a TCP accept succeeds, or
 *  3) a UDP entity is ready.
 *
 *  The same state change callback is invoked when processing is stopped due
 *  to an error or connection close or application close.
 *
 *  @c start can be called, followed by @c stop, followed by @c start, etc.
 *  This may be useful (for example) for a TCP connector that needs to 
 *  reconnect after a connection has been lost.
 *
 *  @param state_chg A function object callback with the following signature:
 *
 *  @code
 *    // TCP io:
 *    void (chops::net::tcp_io_interface, std::error_code, std::size_t);
 *    // UDP io:
 *    void (chops::net::udp_io_interface, std::error_code, std::size_t);
 *  @endcode
 *
 *  The parameters are as follows:
 *
 *  1) provides @c io_interface access (@c start_io, @c stop_io, etc) to the 
 *  underlying IO handler
 *
 *  2) error code, which may be zero (no error) or contain a system error or 
 *  a Chops Net IP error.
 *
 *  3) number of underlying IO handlers associated with this net entity; for 
 *  a TCP connector or a UDP entity the number is either 0 or 1; for a TCP 
 *  acceptor the number is 0 to N, depending on the number of accepted 
 *  connections
 *
 *  The state change function object callback is always invoked at least once
 *  after @c start and before any network processing starts, allowing initialization
 *  of application states (if needed). The values will be:
 *
 *  1) a default constructed @c io_interface (@c is_valid() == @c false) 
 *
 *  2) zero (non-error) std::error_code 
 *
 *  3) 0 (i.e. no connections or binds have occurred)
 *
 *  @return @c true if valid net entity association.
 *
 */
  template <typename F>
  bool start(F&& state_chg) {
    auto p = m_eh_wptr.lock();
    return p ? (p->start(std::forward<F>(state_chg)), true) : false;
  }

/**
 *  @brief Stop network processing on the associated net entity after calling @c stop_io on
 *  each associated IO handler, invoke the application provided state change function object
 *  callback.
 *
 *  Stopping the network processing may involve closing connections, deallocating
 *  resources, and unbinding from ports. 
 *
 *  The state change function object callback will be invoked, possibly multiple times,
 *  with the following values:
 *
 *  1) a valid @c io_interface corresponding to the @c io_interface provided after @c start
 *
 *  2) std::error_code with the value chops::net::net_ip_errc::io_handler_stopped
 *
 *  3) 0 / N (count of valid IO handlers or resources for this @c net_entity)
 *
 *  Specifically, a TCP connector and a UDP net entity will have one state change callback
 *  invocation with a IO handler count of 0, while a TCP acceptor will have a state change
 *  callback invocation for each current connection (and there may be 0 connections), and one 
 *  additional invocation with a default constructed (i.e. invalid) @c io_interface and a 
 *  IO handler count of 0.
 *
 *  @return @c true if valid net entity association.
 *
 */
  void stop() {
    auto p = m_eh_wptr.lock();
    return p ? (p->stop(), true) : false;
  }
/**
 *  @brief Compare two @c net_entity objects for equality.
 *
 *  If both @net_entity objects are valid, then a @c std::shared_ptr comparison is made.
 *  If both @net_entity objects are invalid, @c true is returned (this implies that
 *  all invalid @c net_entity objects are equivalent). If one is valid and the other invalid,
 *  @c false is returned.
 *
 *  @return As described in the comments.
 */

  bool operator==(const net_entity<EH>& rhs) const noexcept {
    auto lp = m_eh_wptr.lock();
    auto rp = rhs.m_eh_wptr.lock();
    return (lp && rp && lp == rp) || (!lp && !rp);
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
  bool operator<(const net_entity<EH>& rhs) const noexcept {
    auto lp = m_eh_wptr.lock();
    auto rp = rhs.m_eh_wptr.lock();
    return (lp && rp && lp < rp) || (!lp && rp);
  }

};

namespace detail {
class tcp_connector;
class tcp_acceptor;
class udp_entity_io;
}

/**
 *  @brief Using declaration for a TCP connector @c net_entity type.
 *
 *  @relates net_entity
 */
using tcp_connector_net_entity = net_entity<detail::tcp_connector>;

/**
 *  @brief Using declaration for a TCP acceptor @c net_entity type.
 *
 *  @relates net_entity
 */
using tcp_acceptor_net_entity = net_entity<detail::tcp_acceptor>;

/**
 *  @brief Using declaration for a UDP based @c net_entity type.
 *
 *  @relates net_entity
 */
using udp_net_entity = net_entity<detail::udp_entity_io>;

} // end net namespace
} // end chops namespace

#endif

