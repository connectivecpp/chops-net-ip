/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief IO type declarations, relating to the @c basic_io_interface and @c basic_io_output 
 *  class templates.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef IO_TYPE_DECLS_HPP_INCLUDED
#define IO_TYPE_DECLS_HPP_INCLUDED

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

namespace chops {
namespace net {

/**
 *  @brief Using declaration for TCP based io, used to instantiate a @c basic_io_interface
 *  or @c basic_io_output type.
 *
 *  @relates basic_io_interface
 *  @relates basic_io_output
 */
using tcp_io = detail::tcp_io;

/**
 *  @brief Using declaration for UDP based io, used to instantiate a @c basic_io_interface
 *  or @c basic_io_output type.
 *
 *  @relates basic_io_interface
 *  @relates basic_io_output
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

/**
 *  @brief Using declaration for a TCP based @c basic_io_output type.
 *
 *  @relates basic_io_output
 */
using tcp_io_output = basic_io_output<tcp_io>;

/**
 *  @brief Using declaration for a UDP based @c basic_io_output type.
 *
 *  @relates basic_io_output
 */
using udp_io_output = basic_io_output<udp_io>;

} // end net namespace
} // end chops namespace

#endif

