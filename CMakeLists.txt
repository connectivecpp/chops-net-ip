# Copyright (c) 2019-2025 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# I'm still learning CMake, so improvement suggestions are always welcome.
#
# The Asio CMake code is taken from CPM.cmake examples/asio-standalone.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.14 FATAL_ERROR )

project ( chops_net_ip
	  LANGUAGES CXX 
	  DESCRIPTION "An asynchronous networking library based on Asio"
	  HOMEPAGE_URL "https://github.com/connectivecpp/chops-net-ip/" )

option ( CHOPS_NET_IP_BUILD_TESTS "Build unit tests" OFF )
option ( CHOPS_NET_IP_BUILD_EXAMPLES "Build examples" OFF )
option ( CHOPS_NET_IP_INSTALL "Install header only library" OFF )

# add library targets

add_library ( chops_net_ip INTERFACE )
add_library ( chops::chops_net_ip ALIAS chops_net_ip )

# thread support specified in Asio download

# dependencies needed for main library

include ( cmake/download_cpm.cmake )
CPMAddPackage ( "gh:connectivecpp/shared-buffer@1.0.4" )
CPMAddPackage ( "gh:martinmoene/expected-lite@0.8.0" )

include ( cmake/download_asio_cpm.cmake )

# configure library target

target_include_directories ( chops_net_ip INTERFACE
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
			     $<INSTALL_INTERFACE:include/> )
target_compile_features ( chops_net_ip INTERFACE cxx_std_20 )

# check to build unit tests
if ( ${CHOPS_NET_IP_BUILD_TESTS} )
  enable_testing()
  add_subdirectory ( test )
endif ()

# check to build example code
if ( ${CHOPS_NET_IP_BUILD_EXAMPLES} )
  add_subdirectory ( example )
endif ()

# check to install
if ( ${CHOPS_NET_IP_INSTALL} )
  set ( CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt )
  include ( CPack )
endif ()

# end of file

