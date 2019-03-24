# Copyright 2019 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.8 )

project ( chops-net-ip-example VERSION 1.0 LANGUAGES CXX )

set ( example_source_dir "${CMAKE_SOURCE_DIR}/example" )

set ( example_sources 
    "${example_source_dir}/aaa_demo.cpp"
    "${example_source_dir}/bbb_demo.cpp" )

set ( OPTIONS "" )
set ( DEFINITIONS "" )

set ( header_dirs
    "${include_source_dir}"
#    "${test_include_dir}"
    )

# Still learning find_package and related ways to bring in third party dependent include directories,
# so don't judge, instead please help.

set ( asio_include_dir "${CMAKE_SOURCE_DIR}/../asio/asio/include" )
if ( NOT $ENV{ASIO_INCLUDE_DIR} STREQUAL "" )
    set ( asio_include_dir $ENV{ASIO_INCLUDE_DIR}} )
endif()
set ( boost_include_dir "${CMAKE_SOURCE_DIR}/../boost_1_69_0" )
if ( NOT $ENV{BOOST_INCLUDE_DIR} STREQUAL "" )
    set ( boost_include_dir $ENV{BOOST_INCLUDE_DIR}} )
endif()
set ( ring_span_lite_include_dir "${CMAKE_SOURCE_DIR}/../ring-span-lite/include" )
if ( NOT $ENV{RING_SPAN_LITE_INCLUDE_DIR} STREQUAL "" )
    set ( ring_span_lite_include_dir $ENV{RING_SPAN_LITE_INCLUDE_DIR}} )
endif()
set ( expected_lite_include_dir "${CMAKE_SOURCE_DIR}/../expected-lite/include" )
if ( NOT $ENV{EXPECTED_LITE_INCLUDE_DIR} STREQUAL "" )
    set ( expected_lite_include_dir $ENV{EXPECTED_LITE_INCLUDE_DIR}} )
endif()

function ( add_target_dependencies target )
#    find_package ( Boost REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${Boost_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${boost_include_dir} )
#    find_package ( asio REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${asio_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${asio_include_dir} )
#    find_package ( ring-span-lite REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${ring-span-lite_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${ring_span_lite_include_dir} )
#    find_package ( expected-lite REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${expected-lite_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${expected_lite_include_dir} )
endfunction()

function ( add_target_info target )
    target_compile_features    ( ${target} PRIVATE cxx_std_17 )
    target_compile_options     ( ${target} PRIVATE ${OPTIONS} )
    target_compile_definitions ( ${target} PRIVATE ${DEFINITIONS} )
    target_include_directories ( ${target} PRIVATE ${header_dirs} )
    add_target_dependencies    ( ${target} )
endfunction()

function ( make_exe target src )
    add_executable        ( ${target} ${src} )
    add_target_info       ( ${target} )
    target_link_libraries ( ${target} PRIVATE pthread )
    message ( "Example executable to create: ${target}" )
    add_test ( NAME ${target} COMMAND ${target} )
endfunction()

foreach ( example_src IN LISTS example_sources )
    get_filename_component ( targ ${example_src} NAME_WE )
    message ( "Calling make_exe for: ${targ}" )
    make_exe ( ${targ} ${example_src} )
endforeach()

# end of file

