/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Various functions that create state change function objects for
 *  common use cases.
 *
 *  Most of these functions are templated on the message handler class, which is
 *  different for every application. The common part logic is calling @c start on a 
 *  @c net_entity with a state change function object and then calling @c start_io on 
 *  an @c io_interface.
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

#include "net_ip/component/simple_variable_len_msg_frame.hpp"

namespace chops {
namespace net {

/**
 *  @brief Create a state change function object that can be passed in to the 
 *  @c net_entity @c start method, later calling @c start_io with a variable length
 *  message frame function object for TCP reads.
 *
 *  The state change function object created does not perform any actions on IO stop.
 *
 *  @param hdr_size Size in bytes of message header.
 *
 *  @param hd_func Header decoder function pointer, as described in @c hdr_decoder_func.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 *  @note This is implemented only for TCP connections.
 */

template <typename MH>
auto make_simple_variable_len_msg_frame_state_change (std::size_t hdr_size, 
                                                      hdr_decoder_func hd_func,
                                                      MH&& msg_hdlr) {
  return [hdr_size, hd_func, mh = std::move(msg_hdlr)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(hdr_size, std::move(mh), make_simple_variable_len_msg_frame(hd_func));
    }
  };
}

/**
 *  @brief Create a state change function object that can be passed in to the 
 *  @c net_entity @c start method, later calling @c start_io with parameters for
 *  TCP delimited reads.
 *
 *  The state change function object created does not perform any actions on IO stop.
 *
 *  @param delim Delimiter for the TCP reads.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 *  @note This is implemented only for TCP connections.
 */

template <typename MH>
auto make_delimiter_read_state_change (std::string_view delim, MH&& msg_hdlr) {
  return [delim, mh = std::move(msg_hdlr)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(delim, std::move(mh));
    }
  };
}

/**
 *  @brief Create a state change function object that can be passed in to the 
 *  @c net_entity @c start method, later calling @c start_io with parameters for
 *  UDP reads or fixed size TCP reads.
 *
 *  The state change function object created does not perform any actions on IO stop.
 *
 *  The IO type is defaulted to UDP, since fixed size TCP reads are a much less common
 *  use case.
 *
 *  @param rd_size Maximum buffer size for UDP reads or size of each TCP read.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 */

template <typename MH, typename IOH = udp_io>
auto make_read_state_change (std::size_t rd_size, MH&& msg_hdlr) {
  return [rd_size, mh = std::move(msg_hdlr)] 
                  (basic_io_interface<IOH> io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(rd_size, std::move(mh));
    }
  };
}

/**
 *  @brief Create a state change function object that can be passed in to the 
 *  @c net_entity @c start method, later calling @c start_io with parameters for
 *  sending only, whether UDP or TCP.
 *
 *  The state change function object created does not perform any actions on IO stop.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 */
template <typename IOH>
auto make_send_only_state_change () {
  return [] (basic_io_interface<IOH> io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io();
    }
  };
}

} // end net namespace
} // end chops namespace

#endif

