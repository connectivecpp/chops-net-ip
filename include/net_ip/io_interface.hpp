/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Declarations for instantiations relating to the @c basic_io_interface
 *  class template.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef IO_INTERFACE_HPP_INCLUDED
#define IO_INTERFACE_HPP_INCLUDED

#include "net_ip/basic_io_interface.hpp"

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

namespace chops {
namespace net {

/**
 *  @brief Using declaration for TCP based io, used to instantiate a @c basic_io_interface
 *  type.
 *
 *  @relates basic_io_interface
 */
using tcp_io = detail::tcp_io;

/**
 *  @brief Using declaration for UDP based io, used to instantiate a @c basic_io_interface
 *  type.
 *
 *  @relates basic_io_interface
 */
using udp_io = detail::udp_entity_io;

/**
 *  @brief Using declaration for a TCP based @c basic_io_interface type.
 *
 *  @relates basic_io_interface
 */
using tcp_io_interface = basic_io_interface<tcp_io>;

/**
 *  @brief Using declaration for a UDP based @c basic_io_interface type.
 *
 *  @relates basic_io_interface
 */
using udp_io_interface = basic_io_interface<udp_io>;

} // end net namespace
} // end chops namespace

#endif

