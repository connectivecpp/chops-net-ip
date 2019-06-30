/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations and implementations for shared test code dealing with test
 *  message building and message handler function object classes.
 *
 *  There are a couple of message handling designs shared between the unit tests.
 *  One is a variable length message, encoded three ways - with a binary length
 *  header, a LF delimited text, and a CR / LF delimited text. The other other is
 *  a fixed-size message.
 *
 *  The general Chops Net IP test strategy for the variable length messages is to 
 *  have message senders and message receivers, with a flag specifying whether the 
 *  receiver is to loop back the messages. For TCP it is independent of whether the 
 *  sender or receiver is an acceptor or connector, although most tests have the 
 *  connector being a sender. In the test routines, coordination is typically needed 
 *  to know when a connection has been made or sender / receiver is ready so that 
 *  message flow can start. At the higher layers, the Chops Net IP library facilities 
 *  provide connection state change function object callbacks.
 *
 *  When the message flow is finished, an empty body message is sent to the receiver
 *  (and looped back if the reply flag is set), which signals an "end of message 
 *  flow" condition. The looped back empty message may not arrive back to the 
 *  sender since connections or handlers are in the process of being taken down.
 *
 *  The fixed-size messages use a simpler messsage flow design, with no "end of
 *  message" indication. This requires a higher layer to bring down the connections
 *  and finish processing.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MSG_HANDLING_HPP_INCLUDED
#define MSG_HANDLING_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint16_t
#include <vector>
#include <utility> // std::forward, std::move
#include <atomic>
#include <ostream>
#include <chrono>
#include <thread>

#include <cassert>
#include <limits>

#include "asio/buffer.hpp"
#include "asio/ip/udp.hpp" // ip::udp::endpoint
#include "asio/ip/address.hpp" // make_address

#include "utility/repeat.hpp"
#include "utility/make_byte_array.hpp"

#include "marshall/extract_append.hpp"
#include "marshall/shared_buffer.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/queue_stats.hpp"

namespace chops {
namespace test {

inline std::size_t decode_variable_len_msg_hdr(const std::byte* buf_ptr, std::size_t sz) {
  assert (sz == 2u);
  return extract_val<std::uint16_t>(buf_ptr);
}

inline chops::mutable_shared_buffer make_body_buf(std::string_view pre, 
                                                  char body_char, 
                                                  std::size_t num_body_chars) {
  chops::mutable_shared_buffer buf(pre.data(), pre.size());
  std::string body(num_body_chars, body_char);
  return buf.append(body.data(), body.size());
}

inline chops::const_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body) {
  assert(body.size() < std::numeric_limits<std::uint16_t>::max());
  std::byte hdr[2];
  auto sz = append_val(hdr, static_cast<std::uint16_t>(body.size()));
  chops::mutable_shared_buffer msg(hdr, 2);
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

constexpr std::size_t fixed_size_buf_size = 33u;

inline chops::const_shared_buffer make_fixed_size_buf() {
  // 33 bytes, mostly consisting of dead beef
  auto ba = chops::make_byte_array(0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                                   0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                                   0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                                   0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                                   0XCC);
  assert (ba.size() == fixed_size_buf_size);
  return chops::const_shared_buffer(ba.data(), ba.size());
}

inline vec_buf make_fixed_size_msg_vec(int num_msgs) {
  vec_buf vec;
  chops::repeat(num_msgs, [&vec] {
      vec.push_back (make_fixed_size_buf());
    }
  );
  return vec;
}

using test_counter = std::atomic_size_t;

template <typename IOT>
struct msg_hdlr {
  using endp_type = typename IOT::endpoint_type;
  using const_buf = asio::const_buffer;

  bool               reply;
  test_counter&      cnt;

  msg_hdlr(bool rep, test_counter& c) : reply(rep), cnt(c) { }

  bool operator()(const_buf buf, chops::net::basic_io_output<IOT> io_out, endp_type endp) {
    chops::const_shared_buffer sh_buf(buf.data(), buf.size());
    if (sh_buf.size() > 2) { // not a shutdown message
      ++cnt;
      if (reply) {
        bool r = io_out.send(sh_buf, endp);
        // assert(r);
      }
      return true;
    }
    if (reply) {
      // may not make it back to sender, depending on TCP connection or UDP reliability
      bool r = io_out.send(sh_buf, endp);
      // assert(r);
    }
    return false;
  }
};

template <typename IOT>
struct fixed_size_msg_hdlr {
  using endp_type = typename IOT::endpoint_type;
  using const_buf = asio::const_buffer;

  test_counter&      cnt;

  fixed_size_msg_hdlr(test_counter& c) : cnt(c) { }

  bool operator()(const_buf buf, chops::net::basic_io_output<IOT> io_out, endp_type endp) {
    assert(buf.size() == fixed_size_buf_size);
    ++cnt;
    return true;
  }

};

class poll_output_queue_cond {
private:
  int m_sleep_time;
  std::ostream& m_log;

public:
  poll_output_queue_cond(int sleep_time, std::ostream& log) noexcept :
        m_sleep_time(sleep_time), m_log(log) { }

  bool operator()(const chops::net::output_queue_stats& stats) const {
    if (stats.output_queue_size == 0u) {
      return true;
    }
    m_log << "Output queue size: " << stats.output_queue_size << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_time));
    return false;
  }

};

} // end namespace test
} // end namespace chops

#endif

