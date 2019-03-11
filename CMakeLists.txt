# Copyright 2019 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# CMake 3.8 required for cxx_std_17 target_compile_features

cmake_minimum_required ( VERSION 3.8 )

option ( CHOPS_NET_IP_OPT_BUILD_TESTS  "Build and perform chops-net-ip tests" ON )
option ( CHOPS_NET_IP_OPT_BUILD_EXAMPLES  "Build and perform chops-net-ip examples" OFF )

project ( chops-net-ip VERSION 1.0 LANGUAGES CXX )

set ( package_name "chops-net-ip" )

set ( include_source_dir "${CMAKE_SOURCE_DIR}/include" )

# Interface library:

add_library ( ${package_name} INTERFACE )

target_include_directories ( ${package_name} INTERFACE ${include_source_dir} )
target_compile_features ( ${package_name} INTERFACE cxx_std_17)

if ( CHOPS_NET_IP_OPT_BUILD_TESTS )
  enable_testing()
  add_subdirectory ( test )
endif()

if ( CHOPS_NET_IP_OPT_BUILD_EXAMPLES )
  enable_testing()
  add_subdirectory ( example )
endif()

# end of file

