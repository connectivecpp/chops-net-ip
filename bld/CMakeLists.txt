cmake_minimum_required (VERSION 3.4)
project (chops)

target_compile_features(chops PUBLIC cxx_std_17)

include_directories("${PROJECT_BINARY_DIR}")
 
# add the executables
add_executable(WaitQueueTest wait_queue_test.cpp)
add_executable(RepeatTest repeat_test.cpp)
