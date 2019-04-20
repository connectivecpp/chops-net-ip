/** @file
 *
 *  @ingroup example_module
 *
 *  @brief Declarations and implementations for variable length binary messages.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef VAR_LEN_HDR_HPP_INCLUDED
#define VAR_LEN_HDR_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::int32_t

#include <cassert>
#include <limits>

namespace chops {
namespace example {

inline std::size_t decode_variable_len_msg_hdr(const std::byte* buf_ptr, std::size_t sz) {
  // code to be filled in
}

template <typename IOT>
struct msg_hdlr {
  using endp_type = typename IOT::endpoint_type;
  using const_buf = asio::const_buffer;

  bool               reply;
  test_counter&      cnt;

  msg_hdlr(bool rep, test_counter& c) : reply(rep), cnt(c) { }

  bool operator()(const_buf buf, chops::net::basic_io_interface<IOT> io_intf, endp_type endp) {
    chops::const_shared_buffer sh_buf(buf.data(), buf.size());
    if (sh_buf.size() > 2) { // not a shutdown message
      ++cnt;
      if (reply) {
        io_intf.send(sh_buf, endp);
      }
      return true;
    }
    if (reply) {
      // may not make it back to sender, depending on TCP connection or UDP reliability
      io_intf.send(sh_buf, endp);
    }
    return false;
  }

};

using tcp_msg_hdlr = msg_hdlr<chops::net::tcp_io>;
using udp_msg_hdlr = msg_hdlr<chops::net::udp_io>;

constexpr int udp_max_buf_size = 65507;

asio::net::ip::udp::endpoint make_udp_endpoint(const char* addr, int port_num) {
  return asio::net::ip::udp::endpoint(asio::net::ip::make_address(addr),
                           static_cast<unsigned short>(port_num));
}


} // end namespace example
} // end namespace chops

#endif

