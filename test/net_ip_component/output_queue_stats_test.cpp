/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c output_queue_stats accumulation functions.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <vector>
#include <list>

#include "net_ip_component/output_queue_stats.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip/io_type_decls.hpp"

#include "shared_test/mock_classes.hpp"

SCENARIO ( "Testing accumulate_output_queue_stats for io_output objects",
           "[accumulate_output_queue_stats]" ) {

  using namespace chops::test;
  using io_out_mock = chops::net::basic_io_output<io_handler_mock>;

  io_handler_mock ioh_mock{};

  io_out_mock io_out1(&ioh_mock);
  auto io_out2 = io_out1;
  auto io_out3 = io_out1;

  std::vector<io_out_mock> io_out_vec { io_out1, io_out2, io_out3 };

  auto s = chops::net::accumulate_output_queue_stats(io_out_vec.cbegin(), io_out_vec.cend());

  REQUIRE (s.output_queue_size == 3*io_handler_mock::qs_base);
  REQUIRE (s.bytes_in_output_queue == 3*(io_handler_mock::qs_base+1));
  
  chops::net::accumulate_output_queue_stats_until(io_out_vec.cbegin(), io_out_vec.cend(),
      [] (const chops::net::output_queue_stats& st) {
        return st.output_queue_size == 3*io_handler_mock::qs_base;
      }
  );

}

SCENARIO ( "Testing accumulate_output_queue_stats for net_entity objects",
           "[accumulate_net_entity_output_queue_stats]" ) {

// Not much runtime testing, as of yet, in this scenario, mostly compile time, using default
// constructed net_entity objects
  chops::net::net_entity ne1;
  auto ne2 = ne1;
  auto ne3 = ne1;
  auto ne4 = ne1;
  std::list<chops::net::net_entity> ne_list { ne1, ne2, ne3, ne4 };

  auto s = chops::net::accumulate_net_entity_output_queue_stats<chops::net::udp_io>(ne_list.cbegin(), 
                                                                                    ne_list.cend());

  REQUIRE (s.output_queue_size == 0u);
  REQUIRE (s.bytes_in_output_queue == 0u);

  chops::net::accumulate_net_entity_output_queue_stats_until<chops::net::udp_io>(ne_list.cbegin(), 
                                                                                 ne_list.cend(),
      [] (const chops::net::output_queue_stats& st) {
        return true;
      }
  );

}


