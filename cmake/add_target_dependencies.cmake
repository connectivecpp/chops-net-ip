# Copyright 2020 by Cliff Green
#
# https://github.com/connectivecpp/utility-rack
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# Still learning find_package and related ways to bring in third party dependent include directories,
# so don't judge, instead please help.

set ( catch2_include_dir "${CMAKE_SOURCE_DIR}/../Catch2/single_include" )
if ( NOT $ENV{CATCH2_INCLUDE_DIR} STREQUAL "" )
    set ( catch2_include_dir $ENV{CATCH2_INCLUDE_DIR}} )
endif()
set ( asio_include_dir "${CMAKE_SOURCE_DIR}/../asio/asio/include" )
if ( NOT $ENV{ASIO_INCLUDE_DIR} STREQUAL "" )
    set ( asio_include_dir $ENV{ASIO_INCLUDE_DIR}} )
endif()

function ( add_target_dependencies target )
#    find_package ( Catch2 REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${Catch2_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${catch2_include_dir} )
#    find_package ( asio REQUIRED )
#    target_include_directories ( ${target} PRIVATE ${asio_INCLUDE_DIRS} )
    target_include_directories ( ${target} PRIVATE ${asio_include_dir} )
endfunction()

# end of file

