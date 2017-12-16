/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Exception class and error category within Chops @c net_ip library.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *
 */

#ifndef NET_IP_ERROR_HPP_INCLUDED
#define NET_IP_ERROR_HPP_INCLUDED

#include <stdexcept>
#include <string>
#include <system_error>

namespace chops {
namespace net {

enum class net_ip_errc {
  message_handler_terminated = 1;
  weak_ptr_expired = 2;
};

namespace std {
  template <>
    struct is_error_code_enum<net_ip_errc> : true_type {};
}

inline std::error_code make_error_code(net_ip_errc e) {
  return { static_cast<int>(e), net_ip_errc };
}

/**
 *  @brief General @c net_ip exception class.
 */
class net_ip_exception : public std::runtime_error {
public:
  net_ip_exception (const std::error_code& e) : 
    std::runtime_error("net_ip lib exception") : err(e) { }

  std::error_code err;
};

} // end net namespace
} // end chops namespace


#endif

