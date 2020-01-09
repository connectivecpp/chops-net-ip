/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Function object class and declaration for simple variable length TCP message 
 *  framing.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef SIMPLE_VARIABLE_LEN_MSG_FRAME_HPP_INCLUDED
#define SIMPLE_VARIABLE_LEN_MSG_FRAME_HPP_INCLUDED

#include "asio/buffer.hpp"

#include <cstddef> // std::size_t, std::byte

#include "utility/cast_ptr_to.hpp"

namespace chops {
namespace net {

/**
 *  @brief Signature for a variable length message header decoder function, used in
 *  one of the @c basic_io_interface @c start_io methods.
 *
 *  Given a buffer of @c std::bytes corresponding to a header on a variable length
 *  message, decode the header and return the length in bytes of the message body.
 *  Specifically, only the size of the body should be returned, not the full message
 *  size (header plus body).
 *
 *  For example, a 14 byte header that contains a full message length would need to
 *  subtract 14 from the length before returning the value from this function.
 *
 *  This can only be a function pointer, not a function object. If state needs to be
 *  stored or more complex logic needed than can be provided by a simple function, then
 *  the @c start_io that takes a full message frame function object should be used.
 *
 *  @param ptr A pointer to the beginning of the variable len message header.
 *
 *  @param sz The size of the header buffer, in bytes, which should be the same for 
 *  every call and match the parameter given to the @c basic_io_interface @c start_io call.
 *
 *  @relates basic_io_interface
 *
 */
using hdr_decoder_func = std::size_t (*)(const std::byte* ptr, std::size_t sz);


/**
 *  @brief Function object class used in the @c basic_io_interface @c start_io methods,
 *  implements a common message framing use case.
 */

class simple_variable_len_msg_frame {
private:
  hdr_decoder_func m_hdr_decoder_func;
  bool             m_hdr_processed;
public:
  simple_variable_len_msg_frame(hdr_decoder_func func) noexcept : 
      m_hdr_decoder_func(func), m_hdr_processed(false) { }

  std::size_t operator() (asio::mutable_buffer buf) noexcept {
    if (!m_hdr_processed) {
      auto sz = m_hdr_decoder_func(cast_ptr_to<std::byte>(buf.data()), buf.size());
      m_hdr_processed = (sz != 0u);
      return sz;
    }
    m_hdr_processed = false;
    return 0u;
  }
};

} // end net namespace
} // end chops namespace

#endif

