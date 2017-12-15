/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Exception classes thrown by Chops @c net_ip library.
 *
 *  @author Cliff Green
 *  @date 2010
 *
 */

#ifndef NET_IP_EXCEPTION_HPP_INCLUDED
#define NET_IP_EXCEPTION_HPP_INCLUDED

#include <stdexcept>
#include <string>

namespace chops {
namespace net {

/**
 *  @brief General @c net_ip exception class.
 */
class net_ip_exception : public std::runtime_error {
public:
  net_ip_exception (const char* what) : 
    std::runtime_error(std::string("net_ip lib exception: ") + std::string(what)) { }
};

/**
 *  @brief Exception class for an invalid network resource.
 */
class net_ip_resource_exception : public std::runtime_error {
public:
  net_ip_resource_exception(const char* what) : 
    std::runtime_error(std::string("net_ip lib exception, resource invalid for: ") + what) { }
};

} // end net namespace
} // end chops namespace


#endif

