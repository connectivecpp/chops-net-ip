/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Error codes, exception class, and error category within 
 *  Chops @c net_ip library.
 *
 *  The @c error_code customization is inspired by blog code from Andrzej Krzemienski and 
 *  Bjorn Reese.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef NET_IP_ERROR_HPP_INCLUDED
#define NET_IP_ERROR_HPP_INCLUDED

#include <stdexcept>
#include <system_error>
#include <string>

namespace chops {
namespace net {

enum class net_ip_errc {
  weak_ptr_expired = 1,
  message_handler_terminated = 2,
  io_state_change_terminated = 3,
  tcp_io_handler_stopped = 4,
  udp_io_handler_stopped = 5,
  tcp_acceptor_stopped = 6,
  tcp_acceptor_closed = 7,
  udp_entity_stopped = 8,
  udp_entity_closed = 9,
  tcp_connector_stopped = 10,
  tcp_connector_closed = 11,
  tcp_connector_resolving_addresses = 12,
  tcp_connector_connecting = 13,
  tcp_connector_connected = 14,
  tcp_connector_timeout = 15,
  functor_variant_mismatch = 16,
};

namespace detail {

struct net_ip_err_category : public std::error_category {
  virtual const char *name() const noexcept override {
    return "net_ip_err_category";
  }
  virtual std::string message(int val) const override {
    switch (net_ip_errc(val)) {
    case net_ip_errc::weak_ptr_expired:
      return "weak pointer expired";
    case net_ip_errc::message_handler_terminated:
      return "message handler terminated via false return value";
    case net_ip_errc::io_state_change_terminated:
      return "io state change terminated via false return value";
    case net_ip_errc::tcp_io_handler_stopped:
      return "tcp io handler stopped";
    case net_ip_errc::udp_io_handler_stopped:
      return "udp io handler stopped";
    case net_ip_errc::tcp_acceptor_stopped:
      return "tcp acceptor stopped";
    case net_ip_errc::tcp_acceptor_closed:
      return "tcp acceptor closed";
    case net_ip_errc::udp_entity_stopped:
      return "udp entity stopped";
    case net_ip_errc::udp_entity_closed:
      return "udp entity closed";
    case net_ip_errc::tcp_connector_stopped:
      return "tcp connector stopped";
    case net_ip_errc::tcp_connector_closed:
      return "tcp connector closed";
    case net_ip_errc::tcp_connector_resolving_addresses:
      return "tcp connector resolving addresses";
    case net_ip_errc::tcp_connector_connecting:
      return "tcp connector connecting";
    case net_ip_errc::tcp_connector_connected:
      return "tcp connector connected";
    case net_ip_errc::tcp_connector_timeout:
      return "tcp connector timeout";
    case net_ip_errc::functor_variant_mismatch:
      return "function object does not match internal variant";
    }
    return "(unknown error)";
  }
};

} // end detail namespace

inline const detail::net_ip_err_category& get_err_category() {
  static detail::net_ip_err_category e;
  return e;
}

} // end net namespace
} // end chops namespace

namespace std {

template <>
  struct is_error_code_enum<chops::net::net_ip_errc> : true_type {};

inline error_code make_error_code(chops::net::net_ip_errc e) noexcept {
  return error_code (static_cast<int>(e), chops::net::get_err_category());
}

}

namespace chops {
namespace net {



/**
 *  @brief General @c net_ip exception class.
 */
class net_ip_exception : public std::runtime_error {
public:
  net_ip_exception (const std::error_code& e) : 
    std::runtime_error("net_ip lib exception"), err(e) { }

  std::error_code err;
};

} // end net namespace
} // end chops namespace


#endif

