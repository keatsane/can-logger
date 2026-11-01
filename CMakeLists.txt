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

# one request, one reply
add_executable(probe src/probe.cpp)
target_link_libraries(probe PRIVATE obd)

# unit tests, built wherever googletest is installed (sudo apt install libgtest-dev)
include(CTest)
find_package(GTest QUIET)
if(BUILD_TESTING AND NOT GTest_FOUND)
  message(STATUS "googletest not found, skipping unit tests")
endif()
if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)
  add_executable(obd_test tests/obd_test.cpp)
  target_link_libraries(obd_test PRIVATE obd GTest::gtest_main)
  gtest_discover_tests(obd_test)
endif()
