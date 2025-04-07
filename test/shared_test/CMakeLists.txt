# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( shared_test LANGUAGES CXX )

set ( test_app_names  io_buf_test
                      mock_classes_test
                      msg_handling_start_funcs_test
                      msg_handling_test )

include ( ../../cmake/test_app_creation.cmake )

enable_testing()

include ( ../../cmake/test_creation.cmake )

