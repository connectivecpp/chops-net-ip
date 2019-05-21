/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Common code for accessing @c std::weak_ptr referenced objects, used in 
 *  @c basic_io_interface and @c net_entity.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef WP_ACCESS_HPP_INCLUDED
#define WP_ACCESS_HPP_INCLUDED

#include <memory> // std::weak_ptr, std::shared_ptr
#include <system_error>

#include "nonstd/expected.hpp"

#include "net_ip/net_ip_error.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename R, typename WP, typename F>
auto wp_helper(const WP& wp, F&& func) ->
      nonstd::expected<R, std::error_code> {
  if (auto sp = wp.lock()) {
    return func(sp);
  }
  return nonstd::make_unexpected(std::make_error_code(net_ip_errc::weak_ptr_expired));
}

template <typename WP, typename F>
auto wp_helper_void(const WP& wp, F&& func) ->
      nonstd::expected<void, std::error_code> {
  if (auto sp = wp.lock()) {
    auto r = func(sp);
    if (r) {
      return nonstd::make_unexpected(r);
    }
    return { };
  }
  return nonstd::make_unexpected(std::make_error_code(net_ip_errc::weak_ptr_expired));
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

