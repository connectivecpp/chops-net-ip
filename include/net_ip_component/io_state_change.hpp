/** @file 
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Functions that create IO state change function objects used in the @c net_entity
 *  @c start method, each will invoke @c start_io.
 *
 *  The common logic in each of these function objects is calling @c start_io on 
 *  an @c io_interface after @c start has been called on a @c net_entity.
 *
 *  In general, there is a creation function corresponding to each overload of the 
 *  @c start_io method of @c basic_io_interface.
 *
 *  Most of these functions are templated on the message handler class, which is
 *  different for every application. 
 *
 *  @note None of these IO state change function object perform any action on 
 *  IO stop or shutdown.
 *
 *  @note These creation functions are not a necessary dependency of the @c net_ip 
 *  core library, but are useful components for many applications.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef IO_STATE_CHANGE_HPP_INCLUDED
#define IO_STATE_CHANGE_HPP_INCLUDED

#include <cstddef> // std::size_t, std::byte
#include <utility> // std::move

#include "asio/ip/udp.hpp" // ip::udp::endpoint

#include "net_ip/io_type_decls.hpp"

#include "net_ip/simple_variable_len_msg_frame.hpp"

namespace chops {
namespace net {

/**
 *  @brief Create an IO state change function object with a simple variable length message 
 *  frame function object for TCP reads.
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
auto make_simple_variable_len_msg_frame_io_state_change (std::size_t hdr_size, 
                                                         MH&& msg_hdlr,
                                                         hdr_decoder_func hd_func) {
  return [hdr_size, hd_func, mh = std::move(msg_hdlr)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(hdr_size, std::move(mh), hd_func);
    }
  };
}

/**
 *  @brief Create an IO state change function object that is fully templatized on 
 *  the message frame function object, versus a simple variable len message frame.
 *
 *  @param hdr_size Size in bytes of message header.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @param msg_frame A function object that can be used as a message frame in the
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 *  @note This is implemented only for TCP connections.
 */

template <typename MH, typename MF>
auto make_msg_frame_io_state_change (std::size_t hdr_size, 
                                     MH&& msg_hdlr,
                                     MF&& msg_frame) {
  return [hdr_size, mh = std::move(msg_hdlr), mf = std::move(msg_frame)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(hdr_size, std::move(mh), std::move(mf));
    }
  };
}

/**
 *  @brief Create an IO state change function object parameters for TCP delimited reads.
 *
 *  The IO state change function object created does not perform any actions on IO stop.
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
auto make_delimiter_read_io_state_change (std::string_view delim, MH&& msg_hdlr) {
  return [delim, mh = std::move(msg_hdlr)] 
                  (tcp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(delim, std::move(mh));
    }
  };
}

/**
 *  @brief Create an IO state change function object with parameters for UDP reads or 
 *  fixed size TCP reads.
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

template <typename MH, typename IOT = udp_io>
auto make_read_io_state_change (std::size_t rd_size, MH&& msg_hdlr) {
  return [rd_size, mh = std::move(msg_hdlr)] 
                  (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(rd_size, std::move(mh));
    }
  };
}

/**
 *  @brief Create an IO state change function object with parameters for sending only, 
 *  whether UDP or TCP.
 *
 *  @return A function object that can be used with the @c start method.
 *
 */
template <typename IOT>
auto make_send_only_io_state_change () {
  return [] (basic_io_interface<IOT> io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io();
    }
  };
}

/**
 *  @brief Create an IO state change function object parameters for UDP senders and
 *  receivers with a default destination endpoint.
 *
 *  @param endp Default destination @c asio::ip::udp::endpoint.
 *
 *  @param max_size Maximum buffer size for UDP reads.
 *
 *  @param msg_hdlr A function object that can be used as a message handler in the 
 *  @c start_io method.
 *
 *  @return A function object that can be used with the @c start method.
 *
 *  @note This is implemented only for UDP processing.
 */

template <typename MH>
auto make_default_endp_io_state_change (asio::ip::udp::endpoint endp,
                                        std::size_t max_size,
                                        MH&& msg_hdlr) {
  return [max_size, endp, mh = std::move(msg_hdlr)] 
                  (udp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(endp, max_size, std::move(mh));
    }
  };
}

/**
 *  @brief Create an IO state change function object parameters for UDP sending only
 *  with a default destination endpoint.
 *
 *  @param endp Default destination @c asio::ip::udp::endpoint.
 *
 *  @return A function object that can be used with the @c start method.
 *
 *  @note This is implemented only for UDP processing.
 */
auto make_send_only_default_endp_io_state_change (asio::ip::udp::endpoint endp) {
  return [endp] (udp_io_interface io, std::size_t num, bool starting) mutable {
    if (starting) {
      io.start_io(endp);
    }
  };
}

} // end net namespace
} // end chops namespace

#endif

