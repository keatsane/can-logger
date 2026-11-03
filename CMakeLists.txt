cmake_minimum_required(VERSION 3.16)
project(can-logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
)
find_package(Threads REQUIRED)

# the thread-safe queue between logger threads, header only
add_library(bounded_queue INTERFACE)
target_include_directories(bounded_queue INTERFACE src)
target_link_libraries(bounded_queue INTERFACE Threads::Threads)

# the iso-tp socket wrapper, linux only
add_library(isotp_socket src/isotp_socket.cpp)
target_include_directories(isotp_socket PUBLIC src)

# the log file on disk, linux only
add_library(log_file src/log_file.cpp)
target_include_directories(log_file PUBLIC src)

# the logger: polls on a schedule, prints a status line, and logs records
add_executable(logger src/logger.cpp)
target_link_libraries(
  logger
  PRIVATE
    bounded_queue
    isotp_socket
    log_file
    obd
    record
    scheduler
    systemd_notify
)

# obd-ii encode/decode, no hardware
add_library(obd src/obd.cpp)
target_include_directories(obd PUBLIC src)

# one request, one reply
add_executable(probe src/probe.cpp)
target_link_libraries(
  probe
  PRIVATE
    isotp_socket
    obd
)

# the on-disk record format and crc32, no hardware
add_library(record src/record.cpp)
target_include_directories(record PUBLIC src)
target_link_libraries(record PUBLIC obd)

# which pid to poll next, no hardware
add_library(scheduler src/scheduler.cpp)
target_include_directories(scheduler PUBLIC src)

# heartbeat/alive messages for watchdog
add_library(systemd_notify src/systemd_notify.cpp)
target_include_directories(systemd_notify PUBLIC src)

# unit tests, built wherever googletest is installed (sudo apt install libgtest-dev)
include(CTest)
find_package(GTest QUIET)
if(BUILD_TESTING AND NOT GTest_FOUND)
  message(STATUS "googletest not found, skipping unit tests")
endif()
if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)
  add_executable(
    unit_tests
    tests/bounded_queue_test.cpp
    tests/log_file_test.cpp
    tests/obd_test.cpp
    tests/record_test.cpp
    tests/scheduler_test.cpp
    tests/systemd_notify_test.cpp
  )
  target_link_libraries(
    unit_tests
    PRIVATE
      bounded_queue
      log_file
      obd
      record
      scheduler
      systemd_notify
      GTest::gtest_main
  )
  # list the tests when ctest runs, not at build time, so `setarch -R` in `just tsan` covers it too
  gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)
endif()
