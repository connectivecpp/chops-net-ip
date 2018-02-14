/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Simple variable length msg frame functionality.
 *
 *  Many variable length TCP messages have a fixed size header followed by a variable 
 *  length body. The @c make_simple_variable_len_msg_frame function creates a function
 *  object that is supplied to the @c basic_io_interface @c start_io method. The 
 *  application provides a function that decodes the message header and returns the size 
 *  of the following message body. The @c make_simple_variable_len_msg_frame_state_change
 *  function creates a function object that can be used with a @c net_entity @c start 
 *  method, and will call @c start_io with appropriate parameters.
 *
 *  @note These functions are not a necessary dependency of the @c net_ip library,
 *  but are useful components in many use cases.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SIMPLE_VARIABLE_LEN_MSG_FRAME_HPP_INCLUDED
#define SIMPLE_VARIABLE_LEN_MSG_FRAME_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <utility> // std::move

#include <experimental/buffer>

#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {

/**
 *  @brief Signature for a variable length message header decoder function.
 *
 *  Given a buffer of @c std::bytes corresponding to a header on a variable length
 *  message, decode the header and return the length in bytes of the message body.
 *
 *  @param ptr A pointer to the beginning of the variable len message header.
 *
 *  @param sz The size of the header buffer, in bytes, which should be the same for 
 *  every call and match the parameter given to the @c io_interface @c start_io call.
 *
 *  @relates make_simple_variable_len_msg_frame
 *
 */
using hdr_decoder_func = std::size_t (*)(const std::byte* ptr, std::size_t sz);

/**
 *  @brief Create a message frame function object for simple variable length messages.
 *
 *  @param func Function pointer (not a function object) that decodes the header.
 *
 *  @return A function object that can be used with the @c start_io method.
 *
 */
inline auto make_simple_variable_len_msg_frame(hdr_decoder_func func) {
  bool hdr_processed = false;
  return [hdr_processed, func] 
      (std::experimental::net::mutable_buffer buf) mutable -> std::size_t {
    return hdr_processed ? 
        (hdr_processed = false, 0) :
        (hdr_processed = true, func(static_cast<const std::byte*>(buf.data()), buf.size()));
  };
}

/**
 *  @brief Create a state change function object that can be passed in to the 
 *  @c net_entity @c start method, later calling @c start_io with appropriate parameters.
 *
 *  @param hdr_size Size in bytes of message header.
 *
 *  @param func Header decoder function pointer, as described in @c hdr_decoder_func.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 */

template <typename MH>
auto make_simple_variable_len_msg_frame_state_change (std::size_t hdr_size, 
                                                      hdr_decoder_func func,
                                                      MH&& msg_hdlr) {
  return [hdr_size, func, mh = std::move(msg_hdlr)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(hdr_size, std::move(mh), make_simple_variable_len_msg_frame(func));
    }
  };
}

} // end net namespace
} // end chops namespace

#endif

