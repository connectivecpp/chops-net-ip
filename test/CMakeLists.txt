# Copyright (c) 2019-2024 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

# create project
project ( periodic_timer_test LANGUAGES CXX )

# add executable
add_executable ( periodic_timer_test periodic_timer_test.cpp )
target_compile_features ( periodic_timer_test PRIVATE cxx_std_20 )

# add dependencies
include ( ../cmake/download_cpm.cmake )

CPMAddPackage ( "gh:catchorg/Catch2@3.6.0" )

set ( CMAKE_THREAD_PREFER_PTHREAD TRUE )
set ( THREADS_PREFER_PTHREAD_FLAG TRUE )
find_package ( Threads REQUIRED )

# link dependencies
target_link_libraries ( periodic_timer_test PRIVATE 
	Threads::Threads periodic_timer asio Catch2::Catch2WithMain )

enable_testing()

add_test ( NAME run_periodic_timer_test COMMAND periodic_timer_test )
set_tests_properties ( run_periodic_timer_test 
  PROPERTIES PASS_REGULAR_EXPRESSION "All tests passed"
  )

