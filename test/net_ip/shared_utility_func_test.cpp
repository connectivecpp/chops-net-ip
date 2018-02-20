/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test the higher level function object utility code shared 
 *  between @c net_ip tests.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <string_view>

#include <experimental/internet>

#include "net_ip/shared_utility_func_test.hpp"

#include "net_ip/net_entity.hpp"

// currently these tests won't run, they are in place for compile testing

SCENARIO ( "Shared Net IP test utility, get_tcp_io_futures",
           "[shared_utility] [tcp_io_futures]" ) {
  using namespace chops::test;

  test_counter cnt;
  auto futs = get_tcp_io_futures(chops::net::tcp_connector_net_entity(), 
                                 true, std::string_view(), cnt);
}

SCENARIO ( "Shared Net IP test utility, get_udp_io_futures",
           "[shared_utility] [udp_io_futures]" ) {
  using namespace chops::test;

  test_counter cnt;
  auto futs1 = get_udp_io_futures(chops::net::udp_net_entity(), 
                                  true, cnt);
  auto futs2 = get_udp_io_futures(chops::net::udp_net_entity(), 
                                  true, cnt, std::experimental::net::ip::udp::endpoint());
}
