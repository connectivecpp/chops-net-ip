# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required (VERSION 3.4)
project (chops)

target_compile_features(chops PUBLIC cxx_std_17)

include_directories("${PROJECT_BINARY_DIR}")
 
# add the executables
add_executable(WaitQueueTest wait_queue_test.cpp)
add_executable(RepeatTest repeat_test.cpp)
