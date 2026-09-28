#
# Copyright (c) 2024, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Prefer an installed GoogleTest and fall back to downloading it.
find_package(GTest 1.10 CONFIG QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GoogleTest not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  )
  set(gtest_force_shared_crt ON)
  set(INSTALL_GTEST OFF)
  set(BUILD_GMOCK OFF)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(pipecat_tests
  test_messages.cpp
  test_rtvi_messages.cpp
  test_utils.cpp
)

set_target_properties(pipecat_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(pipecat_tests PRIVATE pipecat::pipecat GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(pipecat_tests)
