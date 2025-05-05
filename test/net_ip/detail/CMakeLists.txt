# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( net_ip_detail_test LANGUAGES CXX )

set ( test_app_names  io_common_test
                      net_entity_common_test
                      output_queue_test
                      tcp_acceptor_test
                      tcp_connector_test
		      tcp_io_test
		      udp_entity_io_test 
		      wp_access_test )

include ( ../../../cmake/test_app_creation.cmake )

enable_testing()

include ( ../../../cmake/test_creation.cmake )

