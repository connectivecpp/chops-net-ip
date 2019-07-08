/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Function declarations and implementations that create and start shared test 
 *  msg handling objects.
 *
 *  These functions are split out from @c msg_handling.hpp because they bring in more
 *  dependencies. In particular, @c chops::net::tcp_io and @c chops::net::udp_io declarations
 *  are needed, which bring in @c tcp_io and @c udp_entity_io headers in the @c detail
 *  directory.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MSG_HANDLING_START_FUNCS_HPP_INCLUDED
#define MSG_HANDLING_START_FUNCS_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte

#include "asio/ip/udp.hpp" // udp endpoint

#include "net_ip/io_type_decls.hpp"

#include "shared_test/msg_handling.hpp"

namespace chops {
namespace test {

using tcp_msg_hdlr = msg_hdlr<chops::net::tcp_io>;
using udp_msg_hdlr = msg_hdlr<chops::net::udp_io>;
using tcp_fixed_size_msg_hdlr = fixed_size_msg_hdlr<chops::net::tcp_io>;
using udp_fixed_size_msg_hdlr = fixed_size_msg_hdlr<chops::net::udp_io>;

inline auto tcp_start_io (chops::net::tcp_io_interface io, bool reply, 
                   std::string_view delim, test_counter& cnt) {
  return delim.empty() ? 
    io.start_io(2, tcp_msg_hdlr(reply, cnt), decode_variable_len_msg_hdr) :
    io.start_io(delim, tcp_msg_hdlr(reply, cnt));
}

constexpr int udp_max_buf_size = 65507;

inline auto udp_start_io (chops::net::udp_io_interface io, bool reply, test_counter& cnt) {
  return io.start_io(udp_max_buf_size, udp_msg_hdlr(reply, cnt));
}

inline auto udp_start_io (chops::net::udp_io_interface io, bool receiving, test_counter& cnt,
                          const asio::ip::udp::endpoint& remote_endp) {
  return receiving ? 
    io.start_io(remote_endp, udp_max_buf_size, udp_msg_hdlr(false, cnt)) :
    io.start_io(remote_endp);
}

inline asio::ip::udp::endpoint make_udp_endpoint(const char* addr, int port_num) {
  return asio::ip::udp::endpoint(asio::ip::make_address(addr),
                           static_cast<unsigned short>(port_num));
}

} // end namespace test
} // end namespace chops


#endif

