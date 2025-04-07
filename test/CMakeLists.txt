# Copyright (c) 2019-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( chops_net_ip_test LANGUAGES CXX )

# dependencies, Catch2 and Connective C++
CPMAddPackage ( "gh:connectivecpp/utility-rack@1.0.4" )
CPMAddPackage ( "gh:connectivecpp/wait-queue@1.2.3" )
CPMAddPackage ( "gh:connectivecpp/binary-serialize@1.0.4" )

CPMAddPackage ( "gh:catchorg/Catch2@3.8.0" )

add_subdirectory ( net_ip )
add_subdirectory ( net_ip_component )
add_subdirectory ( shared_test )
add_subdirectory ( test_data_blaster )

enable_testing()

