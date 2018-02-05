/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Simple variable length msg frame functionality.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SIMPLE_VARIABLE_LEN_MSG_FRAME_INCLUDED
#define SIMPLE_VARIABLE_LEN_MSG_FRAME_INCLUDED

#include <cstddef> // std::size_t, std::byte

#include <experimental/buffer>

namespace chops {
namespace net {

/**
 *  @brief Signature for a variable length message header decoder function.
 *
 *  Given a buffer of @c std::bytes corresponding to a header on a variable length
 *  message, decode the header and return the length of the message body.
 *
 *  @param ptr A pointer to the beginning of the variable len message header.
 *
 *  @param sz The size of the header buffer, which should be the same for every call 
 *  and match the parameter given to the @c io_interface @c start_io call.
 *
 *  @relates make_simple_variable_len_msg_frame
 *
 *  @note These functions are not a necessary dependency of the @c net_ip library,
 *  but are useful components in many use cases.
 */
using hdr_decoder_func = std::size_t (*)(const std::byte* ptr, std::size_t sz);

/**
 *  @brief Create a message frame function object for simple variable length messages.
 *
 *  Many variable length TCP messages have a fixed size header followed by a variable 
 *  length body. Given a function that can decode the header and return the size of
 *  the following body, this function creates a function object that can be used as 
 *  a message frame in the @c io_interface @c start_io method.
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


} // end net namespace
} // end chops namespace

#endif

