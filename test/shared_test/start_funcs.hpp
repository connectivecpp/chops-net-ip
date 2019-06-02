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

#ifndef START_FUNCS_HPP_INCLUDED
#define START_FUNCS_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte

#include "asio/ip/udp.hpp" // udp endpoint

#include "net_ip/io_type_decls.hpp"
#include "net_ip/simple_variable_len_msg_frame.hpp"
#include "net_ip/net_entity.hpp"

#include "shared_test/msg_handling.hpp"

#include "net_ip_component/io_state_change.hpp"
#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/error_delivery.hpp"

namespace chops {
namespace test {

using tcp_msg_hdlr = msg_hdlr<chops::net::tcp_io>;
using udp_msg_hdlr = msg_hdlr<chops::net::udp_io>;

inline auto tcp_start_io (chops::net::tcp_io_interface io, bool reply, 
                   std::string_view delim, test_counter& cnt) {
  if (delim.empty()) {
    return io.start_io(2, tcp_msg_hdlr(reply, cnt), decode_variable_len_msg_hdr);
  }
  return io.start_io(delim, tcp_msg_hdlr(reply, cnt));
}

constexpr int udp_max_buf_size = 65507;

inline auto udp_start_io (chops::net::udp_io_interface io, bool reply, test_counter& cnt) {
  return io.start_io(udp_max_buf_size, udp_msg_hdlr(reply, cnt));
}

inline auto udp_start_io (chops::net::udp_io_interface io, bool receiving, test_counter& cnt,
                          const asio::ip::udp::endpoint& remote_endp) {
  if (receiving) {
    return io.start_io(remote_endp, udp_max_buf_size, udp_msg_hdlr(false, cnt));
  }
  return io.start_io(remote_endp);
}

inline asio::ip::udp::endpoint make_udp_endpoint(const char* addr, int port_num) {
  return asio::ip::udp::endpoint(asio::ip::make_address(addr),
                           static_cast<unsigned short>(port_num));
}


inline auto get_tcp_io_futures(chops::net::net_entity ent, chops::net::err_wait_q& wq,
                               bool reply, std::string_view delim, test_counter& cnt) {
  if (delim.empty()) {
    return chops::net::make_io_output_future_pair<chops::net::tcp_io>(ent,
           chops::net::make_simple_variable_len_msg_frame_io_state_change(2, 
                                                                          tcp_msg_hdlr(reply, cnt),
                                                                          decode_variable_len_msg_hdr),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
  return chops::net::make_io_output_future_pair<chops::net::tcp_io>(ent,
           chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
}

inline void start_tcp_acceptor(chops::net::net_entity acc, chops::net::err_wait_q& wq,
                               bool reply, std::string_view delim, test_counter& cnt) {

  if (delim.empty()) {
    acc.start(chops::net::make_simple_variable_len_msg_frame_io_state_change(2,
                                                                             tcp_msg_hdlr(reply, cnt),
                                                                             decode_variable_len_msg_hdr),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
  else {
    acc.start(chops::net::make_delimiter_read_io_state_change(delim, tcp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::tcp_io>(wq));
  }
}

inline auto get_udp_io_futures(chops::net::net_entity udp_ent, chops::net::err_wait_q& wq,
                               bool reply, test_counter& cnt) {
  return chops::net::make_io_output_future_pair<chops::net::udp_io>(udp_ent,
           chops::net::make_read_io_state_change(udp_max_buf_size, udp_msg_hdlr(reply, cnt)),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
}

inline auto get_udp_io_futures(chops::net::net_entity udp_ent, chops::net::err_wait_q& wq,
                               bool receiving, test_counter& cnt,
                               const asio::ip::udp::endpoint& remote_endp) {

  if (receiving) {
    return chops::net::make_io_output_future_pair<chops::net::udp_io>(udp_ent,
             chops::net::make_default_endp_io_state_change(remote_endp, 
                                                           udp_max_buf_size, 
                                                           udp_msg_hdlr(false, cnt)),
             chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
  }
  return chops::net::make_io_output_future_pair<chops::net::udp_io>(udp_ent,
           chops::net::make_send_only_default_endp_io_state_change(remote_endp),
           chops::net::make_error_func_with_wait_queue<chops::net::udp_io>(wq));
}

} // end namespace test
} // end namespace chops


#endif

