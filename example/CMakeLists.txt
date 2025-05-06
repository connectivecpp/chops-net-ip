# Copyright 2019-2025 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

project ( chops-net-ip-example LANGUAGES CXX )

set ( example_app_names local_echo_demo
                        chat_server_demo
                        simple_chat_demo
                        echo_binary_text_server_demo
                        echo_binary_text_client_demo
#                        udp_broadcast_demo
                        udp_receiver_demo )

include ( ../cmake/example_app_creation.cmake )

