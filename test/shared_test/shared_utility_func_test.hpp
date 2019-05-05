/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations and implementations for higher level function object utility code 
 *  shared between @c net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef SHARED_UTILITY_FUNC_TEST_HPP_INCLUDED
#define SHARED_UTILITY_FUNC_TEST_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte

#include "asio/ip/udp.hpp" // udp endpoint

#include "net_ip/shared_utility_test.hpp"

#include "net_ip_component/simple_variable_len_msg_frame.hpp"
#include "net_ip_component/io_interface_delivery.hpp"
#include "net_ip_component/io_state_change.hpp"
#include "net_ip_component/error_delivery.hpp"

namespace chops {
namespace test {


inline auto get_tcp_io_futures(chops::net::tcp_connector_net_entity conn, chops::net::err_wait_q& wq,
                               bool reply, std::string_view delim, test_counter& cnt) {
  if (delim.empty()) {
    return chops::net::make_tcp_io_interface_future_pair(conn,
           chops::net::make_simple_variable_len_msg_frame_io_state_change(2, decode_variable_len_msg_hdr,
                                                                          tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
  return chops::net::make_tcp_io_interface_future_pair(conn,
           chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
}

inline void start_tcp_acceptor(chops::net::tcp_acceptor_net_entity acc, chops::net::err_wait_q& wq,
                               bool reply, std::string_view delim, test_counter& cnt) {

  if (delim.empty()) {
    acc.start(chops::net::make_simple_variable_len_msg_frame_io_state_change(2, decode_variable_len_msg_hdr,
                                                                             tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
  else {
    acc.start(chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
}

inline auto get_udp_io_futures(chops::net::udp_net_entity udp_entity, chops::net::err_wait_q& wq,
                               bool reply, test_counter& cnt) {
  return chops::net::make_udp_io_interface_future_pair(udp_entity,
           chops::net::make_read_io_state_change(udp_max_buf_size, udp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
}

inline auto get_udp_io_futures(chops::net::udp_net_entity udp_entity, chops::net::err_wait_q& wq,
                               bool receiving, test_counter& cnt,
                               const asio::ip::udp::endpoint& remote_endp) {

  if (receiving) {
    return chops::net::make_udp_io_interface_future_pair(udp_entity,
             chops::net::make_default_endp_io_state_change(remote_endp, 
                                                           udp_max_buf_size, 
                                                           udp_msg_hdlr(false, cnt)),
             chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
  }
  return chops::net::make_udp_io_interface_future_pair(udp_entity,
           chops::net::make_send_only_default_endp_io_state_change(remote_endp),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
}

} // end namespace test
} // end namespace chops

#endif

