/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Various generic functions and classes for common state change
 *  function object use cases.
 *
 *  Most of these classes are templated on the message handler class, which is
 *  different for every application. However, calling @c start on a @c net_entity 
 *  and calling @c start_io on an @c io_interface frequently follows the same
 *  general form. These classes implement many of the common use cases.
 *
 *  @note These functions are not a necessary dependency of the @c net_ip library,
 *  but are useful components for many applications.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef STATE_CHANGE_HPP_INCLUDED
#define STATE_CHANGE_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <utility> // std::move

#include <experimental/buffer>

#include "net_ip/io_interface.hpp"

namespace chops {
namespace net {

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

