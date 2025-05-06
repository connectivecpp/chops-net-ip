# Copyright 2019-2020 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.12 )

project ( chops-net-ip-test VERSION 1.0 LANGUAGES CXX )

include ( "${cmake_all_repos_include_dir}/unit_test_vars.cmake" )

include ( "${cmake_include_dir}/header_dirs_var.cmake" )
# the following will allow shared_test headers to be picked up by test code
set (header_dirs ${header_dirs} "${CMAKE_SOURCE_DIR}/test" )

set ( test_sources 
    "${test_source_dir}/shared_test/mock_classes_test.cpp"
    "${test_source_dir}/shared_test/msg_handling_test.cpp"
    "${test_source_dir}/shared_test/msg_handling_start_funcs_test.cpp"
    "${test_source_dir}/shared_test/io_buf_test.cpp"
    "${test_source_dir}/net_ip/detail/io_common_test.cpp"
    "${test_source_dir}/net_ip/detail/net_entity_common_test.cpp"
    "${test_source_dir}/net_ip/detail/output_queue_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_acceptor_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_connector_test.cpp"
    "${test_source_dir}/net_ip/detail/tcp_io_test.cpp"
    "${test_source_dir}/net_ip/detail/udp_entity_io_test.cpp"
    "${test_source_dir}/net_ip/detail/wp_access_test.cpp"
    "${test_source_dir}/net_ip_component/error_delivery_test.cpp"
    "${test_source_dir}/net_ip_component/io_output_delivery_test.cpp"
    "${test_source_dir}/net_ip_component/output_queue_stats_test.cpp"
    "${test_source_dir}/net_ip_component/send_to_all_test.cpp"
    "${test_source_dir}/net_ip/basic_io_interface_test.cpp"
    "${test_source_dir}/net_ip/basic_io_output_test.cpp"
    "${test_source_dir}/net_ip/endpoints_resolver_test.cpp"
    "${test_source_dir}/net_ip/net_entity_test.cpp"
    "${test_source_dir}/net_ip/net_ip_error_test.cpp"
    "${test_source_dir}/net_ip/simple_variable_len_msg_frame_test.cpp"
    "${test_source_dir}/net_ip/tcp_connector_timeout_test.cpp"
    "${test_source_dir}/net_ip/net_ip_test.cpp" )

include ( "${cmake_include_dir}/add_target_dependencies.cmake" )

include ( "${cmake_all_repos_include_dir}/add_target_info_func.cmake" )
include ( "${cmake_all_repos_include_dir}/unit_test_main_lib.cmake" )
include ( "${cmake_all_repos_include_dir}/target_exe_func.cmake" )

enable_testing()

foreach ( test_src IN LISTS test_sources )
    get_filename_component ( targ ${test_src} NAME_WE )
    message ( "Calling unit_test_target_exe for: ${targ}" )
    unit_test_target_exe ( ${targ} ${test_src} )
endforeach()

# end of file

