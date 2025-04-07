# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( net_ip_test LANGUAGES CXX )

add_subdirectory ( detail )

set ( test_app_names  basic_io_interface_test
                      basic_io_output_test
                      endpoints_resolver_test
                      net_entity_test
                      net_ip_error_test
		      net_ip_test
		      simple_variable_len_msg_frame_test
                      tcp_connector_timeout_test )

include ( ../../cmake/test_app_creation.cmake )

enable_testing()

include ( ../../cmake/test_creation.cmake )

