/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations for utility code shared between @c net_ip tests.
 *
 *  The general Chops Net IP test strategy is to have a message sender and message 
 *  receiver, with a flag specifying whether the receiver is to loop back the
 *  messages. It is independent of whether the sender or receiver is an acceptor
 *  or connector. In the test routines, coordination may be needed to know when a 
 *  connection has been made so that message flow can start. At the higher layers,
 *  the Chops Net IP library facilities provide the connection notification (state
 *  change function object callbacks).
 *
 *  When the message flow is finished, an empty body message is sent to the receiver. 
 *  This empty body message is sent back to the sender regardless of the loopback
 *  flag, and may make an additional trip to the receiver before being discarded 
 *  (depending on message handler logic).
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include <string_view>
#include <cstddef> // std::size_t
#include <vector>
#include <utility> // std::forward

#include <experimental/buffer>

#include "utility/shared_buffer.hpp"
#include "utility/repeat.hpp"
#include "net_ip/io_interface.hpp"

#ifndef SHARED_UTILITY_TEST_HPP_INCLUDED
#define SHARED_UTILITY_TEST_HPP_INCLUDED

namespace chops {
namespace test {

chops::mutable_shared_buffer make_body_buf(std::string_view pre, char body_char, std::size_t num_body_chars);
chops::mutable_shared_buffer make_variable_len_msg(const chops::mutable_shared_buffer& body);
chops::mutable_shared_buffer make_cr_lf_text_msg(const chops::mutable_shared_buffer& body);
chops::mutable_shared_buffer make_lf_text_msg(const chops::mutable_shared_buffer& body);

std::size_t variable_len_msg_frame(std::experimental::net::mutable_buffer);

using vec_buf = std::vector<chops::mutable_shared_buffer>;

template <typename F>
vec_buf make_msg_set(F&& func, std::string_view pre, char body_char, int num_msgs) {
  vec_buf vec;
  chops::repeat(num_msgs, [&vec, f = std::forward<F>(func), pre, body_char] (const int& i) {
      vec.push_back (f(make_body_buf(pre, body_char, i+1)));
    }
  );
  return vec;
}
template <typename F>
chops::mutable_shared_buffer make_empty_body_msg(F&& func) {
  return func( chops::mutable_shared_buffer{ } );
}

template <typename IOH>
struct msg_hdlr {
  using endp_type = typename IOH::endpoint_type;
  using const_buf = std::experimental::net::const_buffer;

  vec_buf& msgs;
  bool reply;
  bool shutdown_recvd;

  msg_hdlr(vec_buf& m, bool rep) : msgs(m), reply(rep), shutdown_recvd(false) { }

  bool operator()(const_buf buf, chops::net::io_interface<IOH> io_intf, endp_type /* endp */) {
    chops::mutable_shared_buffer sh_buf(buf.data(), buf.size());
    if (sh_buf.size() > 2) { // not a shutdown message
      msgs.push_back(sh_buf);
      return reply ? io_intf.send(std::move(sh_buf)) : true;
    }
    return shutdown_recvd ? false : (shutdown_recvd = true, io_intf.send(std::move(sh_buf)));
  }
};

} // end namespace test
} // end namespace chops

#endif

