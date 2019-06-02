/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the higher level function object utility code shared 
 *  between @c net_ip tests.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <string_view>

#include "asio/ip/udp.hpp"

#include "net_ip/net_entity.hpp"

#include "net_ip_component/error_delivery.hpp"
#include "net_ip_component/io_state_change.hpp"
#include "net_ip_component/io_output_delivery.hpp"

#include "shared_test/msg_handling.hpp"
#include "shared_test/start_funcs.hpp"

// currently these tests won't run, they are in place for compile testing

SCENARIO ( "Shared Net IP test utility, get_tcp_io_futures",
           "[shared_utility] [tcp_io_futures]" ) {
  using namespace chops::test;

  chops::net::err_wait_q wq;
  test_counter cnt;
  auto futs = get_tcp_io_futures(chops::net::net_entity { }, wq,
                                 true, std::string_view(), cnt);
}

SCENARIO ( "Shared Net IP test utility, get_udp_io_futures",
           "[shared_utility] [udp_io_futures]" ) {
  using namespace chops::test;

  chops::net::err_wait_q wq;
  test_counter cnt;
  auto futs1 = get_udp_io_futures(chops::net::net_entity { }, wq,
                                  true, cnt);
  auto futs2 = get_udp_io_futures(chops::net::net_entity { }, wq,
                                  true, cnt, asio::ip::udp::endpoint());
}

SCENARIO ( "Shared Net IP test utility, start TCP acceptor",
           "[shared_utility] [start_tcp_acceptor]" ) {
  using namespace chops::test;

  chops::net::err_wait_q wq;
  test_counter cnt;
  start_tcp_acceptor(chops::net::net_entity { }, wq, 
                     false, std::string_view(), cnt);
}

