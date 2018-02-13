/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Declarations for instantiations relating to the @c basic_net_entity
 *  class template.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef NET_ENTITY_HPP_INCLUDED
#define NET_ENTITY_HPP_INCLUDED

#include "net_ip/basic_net_entity.hpp"

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

namespace chops {
namespace net {

/**
 *  @brief Using declaration for a TCP connector @c basic_net_entity type.
 *
 *  @relates basic_net_entity
 */
using tcp_connector_net_entity = basic_net_entity<detail::tcp_connector>;

/**
 *  @brief Using declaration for a TCP acceptor @c basic_net_entity type.
 *
 *  @relates basic_net_entity
 */
using tcp_acceptor_net_entity = basic_net_entity<detail::tcp_acceptor>;

/**
 *  @brief Using declaration for a UDP based @c basic_net_entity type.
 *
 *  @relates basic_net_entity
 */
using udp_net_entity = basic_net_entity<detail::udp_entity_io>;

} // end net namespace
} // end chops namespace

#endif

