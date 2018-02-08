/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations for utility code shared between @c net_ip tests.
 *
 *  The general Chops Net IP test strategy is to have message senders and message 
 *  receivers, with a flag specifying whether the receiver is to loop back the
 *  messages. For TCP it is independent of whether the sender or receiver is an 
 *  acceptor or connector, although most tests have the connector being a sender. In 
 *  the test routines, coordination is typically needed to know when a connection has 
 *  been made or sender / receiver is ready so that message flow can start. At the 
 *  higher layers, the Chops Net IP library facilities provide connection state
 *  change function object callbacks.
 *
 *  When the message flow is finished, an empty body message is sent to the receiver
 *  (and looped back if the reply flag is set), which signals an "end of message 
 *  flow" condition. The looped back empty message may not arrive back to the 
 *  sender since connections or handlers are in the process of being taken down.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SHARED_UTILITY_TEST_HPP_INCLUDED
#define SHARED_UTILITY_TEST_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <vector>
#include <utility> // std::forward, std::move
#include <atomic>

#include <experimental/buffer>

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/io_interface.hpp"

namespace chops {
namespace test {

chops::mutable_shared_buffer make_body_buf(std::string_view pre, char body_char, std::size_t num_body_chars);

chops::const_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body);
chops::const_shared_buffer make_cr_lf_text_msg(const chops::mutable_shared_buffer& body);
chops::const_shared_buffer make_lf_text_msg(const chops::mutable_shared_buffer& body);

template <typename F>
chops::const_shared_buffer make_empty_body_msg(F&& func) {
  return func( chops::mutable_shared_buffer{ } );
}

inline auto make_empty_variable_len_msg() { return make_empty_body_msg(make_variable_len_msg); }
inline auto make_empty_cr_lf_text_msg() { return make_empty_body_msg(make_cr_lf_text_msg); }
inline auto make_empty_lf_text_msg() { return make_empty_body_msg(make_lf_text_msg); }

using vec_buf = std::vector<chops::const_shared_buffer>;

template <typename F>
vec_buf make_msg_vec(F&& func, std::string_view pre, char body_char, int num_msgs) {
  vec_buf vec;
  chops::repeat(num_msgs, [&vec, f = std::forward<F>(func), pre, body_char] (int i) {
      vec.push_back (f(make_body_buf(pre, body_char, i+1)));
    }
  );
  return vec;
}

std::size_t decode_variable_len_msg_hdr(const std::byte*, std::size_t);

using test_counter = std::atomic_size_t;

template <typename IOH>
struct msg_hdlr {
  using endp_type = typename IOH::endpoint_type;
  using const_buf = std::experimental::net::const_buffer;

  bool               reply;
  test_counter&      cnt;

  msg_hdlr(bool rep, test_counter& c) : reply(rep), cnt(c) { }

  bool operator()(const_buf buf, chops::net::basic_io_interface<IOH> io_intf, endp_type endp) {
    chops::const_shared_buffer sh_buf(buf.data(), buf.size());
    if (sh_buf.size() > 2) { // not a shutdown message
      ++cnt;
      return reply ? io_intf.send(sh_buf, endp) : true;
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

inline bool verify_receiver_count (std::size_t expected, std::size_t actual) {
  return expected == actual;
}

inline bool verify_sender_count (std::size_t total_sent, std::size_t recvd, bool reply) {
  return reply ? total_sent == recvd : recvd == 0;
}

inline void tcp_start_io (chops::net::tcp_io_interface io, bool reply, 
                   std::string_view delim, test_counter& cnt) {
  if (delim.empty()) {
    io.start_io(2, tcp_msg_hdlr(reply, cnt), 
                chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr));
  }
  else {
    io.start_io(delim, tcp_msg_hdlr(reply, cnt));
  }
}

constexpr int udp_max_buf_size = 65507;

inline void udp_start_io (chops::net::udp_io_interface io, bool reply, test_counter& cnt) {
  io.start_io(udp_max_buf_size, udp_msg_hdlr(reply, cnt));
}


} // end namespace test
} // end namespace chops

#endif

