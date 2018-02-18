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
#include <experimental/internet> // ip::udp::endpoint

#include <boost/endian/conversion.hpp>

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/io_interface.hpp"

#include "net_ip/component/simple_variable_len_msg_frame.hpp"

namespace chops {
namespace test {

inline chops::mutable_shared_buffer make_body_buf(std::string_view pre, 
                                                  char body_char, 
                                                  std::size_t num_body_chars) {
  chops::mutable_shared_buffer buf(pre.data(), pre.size());
  std::string body(num_body_chars, body_char);
  return buf.append(body.data(), body.size());
}

inline chops::const_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body) {
  std::uint16_t hdr = boost::endian::native_to_big(static_cast<std::uint16_t>(body.size()));
  chops::mutable_shared_buffer msg(static_cast<const void*>(&hdr), 2);
  return chops::const_shared_buffer(std::move(msg.append(body.data(), body.size())));
}

inline chops::const_shared_buffer make_cr_lf_text_msg(const chops::mutable_shared_buffer& body) {
  chops::mutable_shared_buffer msg(body.data(), body.size());
  auto ba = chops::make_byte_array(0x0D, 0x0A); // CR, LF
  return chops::const_shared_buffer(std::move(msg.append(ba.data(), ba.size())));
}

inline chops::const_shared_buffer make_lf_text_msg(const chops::mutable_shared_buffer& body) {
  chops::mutable_shared_buffer msg(body.data(), body.size());
  auto ba = chops::make_byte_array(0x0A); // LF
  return chops::const_shared_buffer(std::move(msg.append(ba.data(), ba.size())));
}

inline std::size_t decode_variable_len_msg_hdr(const std::byte* buf_ptr, std::size_t /* sz */) {
  // assert (sz == 2);
  std::uint16_t hdr;
  std::byte* hdr_ptr = static_cast<std::byte*>(static_cast<void*>(&hdr));
  *(hdr_ptr+0) = *(buf_ptr+0);
  *(hdr_ptr+1) = *(buf_ptr+1);
  return boost::endian::big_to_native(hdr);
}

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

inline void udp_start_io (chops::net::udp_io_interface io, bool receiving, test_counter& cnt,
                          const std::experimental::net::ip::udp::endpoint& remote_endp) {
  if (receiving) {
    io.start_io(udp_max_buf_size, remote_endp, udp_msg_hdlr(false, cnt));
  }
  else {
    io.start_io(remote_endp);
  }
}

struct io_handler_mock {
  using socket_type = int;
  using endpoint_type = double;
  using io_type = short;

  socket_type sock = 3;
  bool started = false;
  constexpr static std::size_t qs_base = 42;

  bool is_io_started() const { return started; }

  socket_type& get_socket() { return sock; }

  chops::net::output_queue_stats get_output_queue_stats() const { 
    return chops::net::output_queue_stats { qs_base, qs_base +1 };
  }

  void send(chops::const_shared_buffer) { }
  void send(chops::const_shared_buffer, const endpoint_type&) { }

  template <typename MH, typename MF>
  void start_io(std::size_t, MH&&, MF&&) { started = true; }

  template <typename MH>
  void start_io(std::string_view, MH&&) { started = true; }

  template <typename MH>
  void start_io(std::size_t, MH&&) { started = true; }

  template <typename MH>
  void start_io(std::size_t, const endpoint_type&, MH&&) { started = true; }

  void start_io() { started = true; }

  void start_io(const endpoint_type&) { started = true; }

  void stop_io() { started = false; }

};

struct net_entity_mock {
  using socket_type = double;

  constexpr static double special_val = 42.0;
  double dummy = special_val;

  bool started = false;

  bool is_started() const { return started; }

  double& get_socket() { return dummy; }

  template <typename F1, typename F2>
  void start( F1&&, F2&& ) { started = true; }

  void stop() { started = false; }

};


} // end namespace test
} // end namespace chops

#endif

