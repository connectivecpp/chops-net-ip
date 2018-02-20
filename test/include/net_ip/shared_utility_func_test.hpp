/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Declarations and implementations for utility code shared between @c net_ip tests 
 *  that uses higher level function objects.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SHARED_UTILITY_FUNC_TEST_HPP_INCLUDED
#define SHARED_UTILITY_FUNC_TEST_HPP_INCLUDED

#include <string_view>
#include <cstddef> // std::size_t, std::byte
#include <utility> // std::forward, std::move
#include <memory> // std::shared_ptr
#include <system_error>

#include "net_ip/io_interface.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip/shared_utility_test.hpp"

#include "net_ip/component/simple_variable_len_msg_frame.hpp"
#include "net_ip/component/io_interface_delivery.hpp"
#include "net_ip/component/io_state_change.hpp"

namespace chops {
namespace test {


inline auto get_tcp_io_futures(chops::net::tcp_connector_net_entity conn,
                               bool reply, std::string_view delim, test_counter& cnt) {
  if (delim.empty()) {
    return chops::net::make_tcp_io_interface_future_pair(conn,
             chops::net::make_simple_variable_len_msg_frame_io_state_change(2,
                                                                            decode_variable_len_msg_hdr,
                                                                            tcp_msg_hdlr(reply, cnt)));
  }
}

} // end namespace test
} // end namespace chops

#endif

