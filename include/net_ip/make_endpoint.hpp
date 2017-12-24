/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Implementation detail code to create an @c ip::basic_endpoint from various
 *  parameters. Name resolving will be performed as needed.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef MAKE_ENDPOINT_HPP_INCLUDED
#define MAKE_ENDPOINT_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/io_context>

#include <string_view>
#include <system_error>

#include "net_ip/net_ip_exception.hpp"

namespace chops {
namespace net {
namespace detail {

/**
 *  @brief Implementation detail utility function which creates an @c ip::basic_endpoint from host 
 *  name strings and port numbers.
 *
 *  The @c ip::basic_endpoint returned from this function is used within other @c net_ip layers.
 *
 *  DNS lookup (resolving) will not be performed if the host name is already in dotted numeric 
 *  or hexadecimal (V6) form. The first entry will be used from a DNS lookup if multiple IP 
 *  addresses are returned.
 * 
 *  If a DNS resolve needs to be performed,
 *
 *  @param addr A host name, which can be empty (which means the address field of the endpoint 
 *  is not set, which is usually interpreted as an "any" address), either in dotted numeric form 
 *  in which case no DNS lookup performed, or a string name where a DNS lookup will be performed.
 *
 *  @param port_num The port number to be set in the endpoint and zero means the port is not set.
 *
 *  @param ioc @c std::experimental::net::io_context, for DNS lookup.
 *
 *  @param ipv4_only If @c true, only resolve DNS entries for ipv4.
 *
 *  @throw @c net_ip_exception if address is unable to be resolved.
 */

template <typename Protocol>
std::experimental::net::ip::basic_endpoint<Protocol> make_endpoint(std::experimental::net::io_context& ioc,
      std::string_view addr, unsigned short port_num, bool ipv4_only = false) {

  std::experimental::net::ip::basic_endpoint<Protocol> endp;
  if (portNum != 0) {
    endp.port(portNum);
  }
  if (addr.empty()) { // port is only important info (should not be 0), no resolve needed
    return endp;
  }
  std::error_code ec;
  endp.address(std::experimental::ip::address::make_address(addr, ec));
  if (!ec) { // no lookup needed, already an address
    return endp;
  }
  // need to resolve, since make_address returned an error
  std::experimental::net::ip::basic_resolver<Protocol> resolver(ioc);
  auto res = resolver.query(addr,""); // throw an exception if not found
  if (!ipv4_only) {
    endp.address((*res.cbegin()).endpoint().address());
    return endp;
  }
  for (const auto& entry : res) {
    endp.address((*entry.cbegin()).endpoint().address());
  }
  throw chops::net::net_ip_exception("No DNS match found");
}

}  // end detail namespace
}  // end net namespace
}  // end chops namespace

#endif

