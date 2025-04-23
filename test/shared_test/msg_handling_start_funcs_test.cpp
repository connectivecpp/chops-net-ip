/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the msg handling start function utility code shared 
 *  between @c net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch_test_macros.hpp"

#include <string_view>

#include "asio/ip/udp.hpp"

#include "net_ip/net_entity.hpp"

#include "net_ip_component/error_delivery.hpp"
#include "net_ip_component/io_state_change.hpp"
#include "net_ip_component/io_output_delivery.hpp"

#include "shared_test/msg_handling_start_funcs.hpp"

// currently nothing in place

TEST_CASE ( "Shared Net IP test utility, msg handling start functions",
           "[shared_utility] [msg_handling]" ) {
  using namespace chops::test;

}

