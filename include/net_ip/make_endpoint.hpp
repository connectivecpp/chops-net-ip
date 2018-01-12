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
#include <experimental/executor>

#include <string_view>
#include <system_error>
#include <utility> // std::forward

#include "net_ip/net_ip_error.hpp"

namespace chops {
namespace net {

/**
 *  @brief Create an endpoint and return it in a function object callback, performing
 *  name resolving (DNS lookup) if needed.
 *
 *  Name resolving will not be performed if the host name is already in dotted numeric or 
 *  hexadecimal (ipV6) form, or if the host name is empty (common for when the local
 *  host IP address is "INADDR_ANY"). 
 *
 *  If name resolving is performed, the first endpoint entry will be used if multiple IP 
 *  addresses are found.
 *
 *  This function always returns before the function object callback is invoked, regardless of
 *  whether the endpoint is immediately ready or not.
 *
 *  It is the applications responsibility to ensure that the data is still valid when the
 *  callback is invoked. A typical idiom is to use @c std::shared_from_this as part of the 
 *  function object callback data.
 *
 *  @tparam Protocol Either @c std::experimental::net::ip::tcp or 
 *  @c std::experimental::net::ip::udp.
 *
 *  @param ioc @c std::experimental::net::io_context, for function object posting and name
 *  resolving (as needed).
 *
 *  @param func Function object which will be invoked when the name resolution completes. The
 *  signature of the callback:
 *  @code
 *    // TCP:
 *    void (const std::error_code& err, std::experimental::net::ip::tcp::endpoint);
 *    // UDP:
 *    void (const std::error_code& err, std::experimental::net::ip::udp::endpoint);
 *  @endcode
 *  If an error has occurred, the error code is set accordingly. 
 *
 *  @param addr A host name, which can be empty (which means the address field of the endpoint 
 *  is not set, interpreted internally as an "INADDR_ANY" address).
 *
 *  @param port_num The port number to be set in the endpoint; zero means the port is not set.
 *
 */
template <typename Protocol, typename F>
void make_endpoint(std::experimental::net::io_context& ioc, F&& func,
      std::string_view addr, unsigned short port_num) {

  using namespace std::experimental::net;
  using results_t = typename ip::basic_resolver<Protocol>::results_type;

  ip::basic_endpoint<Protocol> endp;
  if (port_num != 0) {
    endp.port(port_num);
  }
  if (addr.empty()) { // port is only important info (should not be 0), no resolve needed
    post(ioc.get_context(), [endp, f = std::forward<F>(func)] () { f(std::error_code(), endp); });
    return;
  }
  std::error_code ec;
  endp.address(ip::address::make_address(addr, ec));
  if (!ec) { // was able to make address, addr was in dotted or hex form
    post(ioc.get_context(), [endp, f = std::forward<F>(func)] () { f(std::error_code(), endp); } );
    return;
  }
  // need to resolve, since make_address returned an error
  ip::basic_resolver<Protocol> resolver(ioc);
  resolver.async_resolve(addr, "", [endp, f = std::forward<F>(func)] 
                                   (const std::error_code& err, results_t results) {
      if (err) {
        f(err, endp);
        return;
      }
      if (
      else {
        endp.address((*results.cbegin()).endpoint().address());
        f(std::error_code(), endp);
      }
    }
  );
  return;
}

}  // end net namespace
}  // end chops namespace

#endif

