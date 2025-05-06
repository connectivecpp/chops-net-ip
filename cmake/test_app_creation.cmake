# Copyright (c) 2024-2025 by Cliff Green
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

# add unit test executable
foreach ( test_app_name IN LISTS test_app_names )
  message ( "Creating test executable: ${test_app_name}" )
  add_executable ( ${test_app_name} ${test_app_name}.cpp )
  target_compile_features ( ${test_app_name} PRIVATE cxx_std_20 )
  target_include_directories ( ${test_app_name} PRIVATE
                               ${CMAKE_SOURCE_DIR}/test/ )
  target_link_libraries ( ${test_app_name} PRIVATE 
      Threads::Threads 
      chops_net_ip asio expected-lite shared_buffer 
      utility_rack wait_queue binary_serialize
      Catch2::Catch2WithMain )
endforeach()

