# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( test_data_blaster LANGUAGES CXX )

set ( test_app_names  dsr_args_test
                      monitor_connector_test
                      monitor_msg_test )

include ( ../../cmake/test_app_creation.cmake )

enable_testing()

include ( ../../cmake/test_creation.cmake )

