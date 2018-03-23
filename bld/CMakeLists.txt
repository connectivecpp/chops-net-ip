cmake_minimum_required (VERSION 3.0)
project (Chops VERSION 0.2 LANGUAGES CXX)

include_directories("${PROJECT_BINARY_DIR}")
 
# add the executables
add_executable(WaitQueueTest wait_queue_test.cpp)
add_executable(RepeatTest repeat_test.cpp)
