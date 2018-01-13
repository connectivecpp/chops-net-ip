/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Functions to convert network host names and ports into C++ Networking TS
 *  endpoint objects.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef MAKE_ENDPOINTS_HPP_INCLUDED
#define MAKE_ENDPOINTS_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/io_context>

#include <string_view>
#include <string>

namespace chops {
namespace net {

/**
 *  @brief Create a sequence of endpoints and return them in a function object callback, 
 *  performing name resolving (DNS lookup) as needed.
 *
 *  This function is used in internal Chops Net IP layers, but is available for application 
 *  use if needed.
 *
 *  Many times only one endpoint is needed, for example a TCP acceptor local endpoint or a UDP 
 *  local endpoint. In this case the first entry (if there are many) can be used.
 *
 *  Name resolving will not be performed if the host name is already in dotted numeric or 
 *  hexadecimal (ipV6) form, or if the host name is empty (common for when the local
 *  host IP address is "INADDR_ANY" and used for a TCP acceptor local endpoint). 
 *
 *  This function always returns before the function object callback is invoked.
 *
 *  It is the applications responsibility to ensure that the callback context is still valid 
 *  when the callback is invoked. A typical idiom is to use @c std::shared_from_this as part 
 *  of the function object callback member.
 *
 *  @tparam Protocol Either @c std::experimental::net::ip::tcp or 
 *  @c std::experimental::net::ip::udp.
 *
 *  @param ioc @c std::experimental::net::io_context, for asynchronous processing.
 *
 *  @param func Function object which will be invoked when the name resolution completes. The
 *  signature of the callback:
 *
 *  @code
 *    using namespace std::experimental::net;
 *    // TCP:
 *    void (const std::error_code& err, ip::basic_resolver_results<ip::tcp>);
 *    // UDP:
 *    void (const std::error_code& err, ip::basic_resolver_results<ip::udp>);
 *  @endcode
 *
 *  If an error occurs, the error code is set accordingly. 
 *
 *  @param local If @c true, create endpoints for a local endpoint; specifically the 
 *  "passive" flag is set.
 *
 *  @param host_or_intf_name A host or interface name; if empty it can be used for a local 
 *  endpoint where it specifies an "any" ("INADDR_ANY") address; if not empty it is used for the 
 *  remote host name or for a local interface address.
 *
 *  @param service_or_port A service name or port number; if empty all resolved endpoints will 
 *  have a port number of zero.
 *
 */
template <typename Protocol, typename F>
void make_endpoints(std::experimental::net::io_context& ioc, F&& func, bool local,
      std::string_view host_or_intf_name, std::string_view service_or_port) {

  using namespace std::experimental::net;

// Chris' Networking TS is not yet recognizing string_view, so use string for parms
  ip::basic_resolver<Protocol> res(ioc);
  if (local) {
    ip::resolver_base::flags f (ip::resolver_base::passive | ip::resolver_base::address_configured);
    res.async_resolve(std::string(host_or_intf_name), std::string(service_or_port), f, func);
  }
  else {
    res.async_resolve(std::string(host_or_intf_name), std::string(service_or_port), func);
  }
  return;
}

template <typename Protocol>
void make_endpoints(std::experimental::net::io_context& ioc, F&& func, bool local,
      std::string_view host_or_intf_name, std::string_view service_or_port) {

  using namespace std::experimental::net;

// Chris' Networking TS is not yet recognizing string_view, so use string for parms
  ip::basic_resolver<Protocol> res(ioc);
  if (local) {
    ip::resolver_base::flags f (ip::resolver_base::passive | ip::resolver_base::address_configured);
    res.async_resolve(std::string(host_or_intf_name), std::string(service_or_port), f, func);
  }
  else {
    res.async_resolve(std::string(host_or_intf_name), std::string(service_or_port), func);
  }
  return;
}

}  // end net namespace
}  // end chops namespace

#endif

