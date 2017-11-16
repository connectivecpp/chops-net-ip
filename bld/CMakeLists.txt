cmake_minimum_required (VERSION 2.8)
project (ChopsTests VERSION 0.8 LANGUAGES CXX)

include_directories("${PROJECT_BINARY_DIR}")
 
# add the executables
add_executable(WaitQueueTest wait_queue_test.cpp)
add_executable(RepeatTest repeat_test.cpp)
