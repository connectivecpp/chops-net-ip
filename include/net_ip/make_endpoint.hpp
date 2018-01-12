/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Function to convert network names and ports into C++ Networking TS
 *  endpoint objects, with DNS name resolving performed as needed.
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
#include <optional>

#include "net_ip/net_ip_error.hpp"

namespace chops {
namespace net {

using opt_endpoint = std::optional< std::experimental::net::ip::basic_endpoint<Protocol> >;

/**
 *  @brief If possible, create an @c ip::basic_endpoint from a host name string and port number.
 *
 *  Return an endpoint if name resolution is not needed. If name resolving is needed (i.e. a 
 *  DNS lookup), the endpoint is not returned, and the @c resolve_endpoint function will need to 
 *  be called.
 *
 *  Name resolving will not be performed if the host name is already in dotted numeric 
 *  or hexadecimal (ipV6) form. 
 *
 *  @param addr A host name, which can be empty (which means the address field of the endpoint 
 *  is not set, which is usually interpreted as an "any" address), either in dotted numeric form 
 *  in which case no DNS lookup performed, or a string name where a DNS lookup will be performed.
 *
 *  @param port_num The port number to be set in the endpoint, where zero means the port is not 
 *  set.
 *
 *  @return If a value is present (in the @c std::optional), a valid endpoint is ready for use,
 *  otherwise a name resolution is needed.
 *
 */
template <typename Protocol>
opt_endpoint make_endpoint(std::string_view addr, unsigned short port_num) {

  std::experimental::net::ip::basic_endpoint<Protocol> endp;
  if (port_num != 0) {
    endp.port(port_num);
  }
  if (addr.empty()) { // port is only important info (should not be 0), no resolve needed
    return opt_endpoint { endp };
  }
  std::error_code ec;
  endp.address(std::experimental::ip::address::make_address(addr, ec));
  return ec ? opt_endpoint { } : opt_endpoint { endp };
}

/**
 *  @brief Perform name resolving and return an endpoint in a function object callback.
 *
 *  The first entry will be used from a DNS lookup if multiple IP 
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
 */
template <typename Protocol, typename F>
void resolve_endpoint(std::experimental::net::io_context& ioc, F&& func,
      std::string_view addr, unsigned short port_num, bool ipv4_only = false) {
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
}

}  // end net namespace
}  // end chops namespace

#endif

