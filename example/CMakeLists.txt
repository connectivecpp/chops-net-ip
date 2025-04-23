# Copyright 2019-2025 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

project ( chops-net-ip-example VERSION 1.0 LANGUAGES CXX )

set ( example_app_names local_echo_demo
                        chat_server_demo
                        simple_chat_demo
                        echo_binary_text_server_demo
                        echo_binary_text_client_demo
                        udp_broadcast_demo
                        udp_receiver_demo )

set ( CMAKE_THREAD_PREFER_PTHREAD TRUE )
set ( THREADS_PREFER_PTHREAD_FLAG TRUE )
find_package ( Threads REQUIRED )

# add executables
foreach ( example_app_name IN LISTS example_app_names )
  message ( "Creating example executable: ${example_app_name}" )
  add_executable ( ${example_app_name} ${example_app_name}.cpp )
  target_compile_features ( ${example_app_name} PRIVATE cxx_std_20 )
  target_link_libraries ( ${example_app_name} 
          PRIVATE Threads::Threads asio shared_buffer chops_net_ip )
endforeach()

# end of file

