cmake_minimum_required(VERSION 3.16)
project(can-logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

# obd-ii encode/decode, no hardware
add_library(obd src/obd.cpp)
target_include_directories(obd PUBLIC src)

# which pid to poll next, no hardware
add_library(scheduler src/scheduler.cpp)
target_include_directories(scheduler PUBLIC src)

# the iso-tp socket wrapper, linux only
add_library(isotp_socket src/isotp_socket.cpp)
target_include_directories(isotp_socket PUBLIC src)

# one request, one reply
add_executable(probe src/probe.cpp)
target_link_libraries(probe PRIVATE obd isotp_socket)

# the logger: polls on a schedule, prints a status line
add_executable(logger src/logger.cpp)
target_link_libraries(logger PRIVATE obd isotp_socket scheduler)

# unit tests, built wherever googletest is installed (sudo apt install libgtest-dev)
include(CTest)
find_package(GTest QUIET)
if(BUILD_TESTING AND NOT GTest_FOUND)
  message(STATUS "googletest not found, skipping unit tests")
endif()
if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)
  add_executable(unit_tests tests/obd_test.cpp tests/scheduler_test.cpp)
  target_link_libraries(unit_tests PRIVATE obd scheduler GTest::gtest_main)
  gtest_discover_tests(unit_tests)
endif()
