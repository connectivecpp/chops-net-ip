# Copyright 2019 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.8 )

project ( chops-net-ip-test VERSION 1.0 LANGUAGES CXX )

set ( test_source_dir "${CMAKE_SOURCE_DIR}/test" )
set ( test_main_src "${test_source_dir}/main_test.cpp" )
set ( test_include_dir "${test_source_dir}/shared_test" )
set ( tester_suffix "_runner" )
set ( main_test_lib_name "main_test_lib" )

set ( test_sources 
    "${test_source_dir}/shared_test/mock_classes_test.cpp"
    "${test_source_dir}/shared_test/msg_handling_test.cpp"
    "${test_source_dir}/net_ip/detail/io_common_test.cpp"
    "${test_source_dir}/net_ip/detail/net_entity_common_test.cpp"
    "${test_source_dir}/net_ip/detail/output_queue_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_acceptor_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_connector_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_io_test.cpp"
    "${test_source_dir}/net_ip/detail/udp_entity_io_test.cpp"
    "${test_source_dir}/net_ip/net_ip_component/error_delivery_test.cpp"
    "${test_source_dir}/net_ip/net_ip_component/io_interface_delivery_test.cpp"
    "${test_source_dir}/net_ip/basic_io_interface_test.cpp"
    "${test_source_dir}/net_ip/basic_io_output_test.cpp"
    "${test_source_dir}/net_ip/endpoints_resolver_test.cpp"
    "${test_source_dir}/net_ip/net_entity_test.cpp"
    "${test_source_dir}/net_ip/net_ip_error_test.cpp"
    "${test_source_dir}/net_ip/simple_variable_len_msg_frame_test.cpp"
    "${test_source_dir}/net_ip/net_ip_test.cpp" )

set ( OPTIONS "" )
set ( DEFINITIONS "" )

set ( header_dirs
    "${include_source_dir}"
    "${test_include_dir}"
    )

# Still learning find_package and related ways to bring in third party dependent include directories,
# so don't judge, instead please help.

set ( catch2_include_dir "${CMAKE_SOURCE_DIR}/../Catch2/single_include" )
if ( NOT $ENV{CATCH2_INCLUDE_DIR} STREQUAL "" )
    set ( catch2_include_dir $ENV{CATCH2_INCLUDE_DIR}} )
endif()
set ( utility_rack_include_dir "${CMAKE_SOURCE_DIR}/../utility-rack/include" )
if ( NOT $ENV{UTILITY_RACK_INCLUDE_DIR} STREQUAL "" )
    set ( utility_rack_include_dir $ENV{UTILITY_RACK_INCLUDE_DIR}} )
endif()
set ( asio_include_dir "${CMAKE_SOURCE_DIR}/../asio/asio/include" )
if ( NOT $ENV{ASIO_INCLUDE_DIR} STREQUAL "" )
    set ( asio_include_dir $ENV{ASIO_INCLUDE_DIR}} )
endif()
set ( expected_lite_include_dir "${CMAKE_SOURCE_DIR}/../expected-lite/include" )
if ( NOT $ENV{EXPECTED_LITE_INCLUDE_DIR} STREQUAL "" )
    set ( expected_lite_include_dir $ENV{EXPECTED_LITE_INCLUDE_DIR}} )
endif()

function ( add_target_dependencies target )
#    find_package ( utility-rack REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${utility-rack_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${utility_rack_include_dir} )
#    find_package ( Catch2 REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${Catch2_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${catch2_include_dir} )
#    find_package ( asio REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${asio_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${asio_include_dir} )
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

add_library ( ${main_test_lib_name} STATIC ${test_main_src} )
add_target_info ( ${main_test_lib_name} )

function ( make_exe target src )
    add_executable        ( ${target} ${src} )
    add_target_info       ( ${target} )
    target_link_libraries ( ${target} PRIVATE pthread )
    target_link_libraries ( ${target} PRIVATE ${main_test_lib_name} )
    message ( "Test executable to create: ${target}" )
    add_test ( NAME ${target}${tester_suffix} COMMAND ${target} )
endfunction()

enable_testing()

foreach ( test_src IN LISTS test_sources )
    get_filename_component ( targ ${test_src} NAME_WE )
    message ( "Calling make_exe for: ${targ}" )
    make_exe ( ${targ} ${test_src} )
endforeach()

# end of file

