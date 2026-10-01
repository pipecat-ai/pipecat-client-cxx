#
# Copyright (c) 2024-2026, Daily
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

add_executable(pipecat_daily_tests
  test_audio_buffer.cpp
  test_session.cpp
  test_transport.cpp
)

set_target_properties(pipecat_daily_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Tests also cover private code in src/.
target_include_directories(pipecat_daily_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(pipecat_daily_tests PRIVATE
  pipecat::daily
  GTest::gtest_main
)

# Daily Core is a shared library. On Windows, the tests need it next to them to
# run, and gtest_discover_tests() runs them right after they're built.
if(WIN32)
  add_custom_command(TARGET pipecat_daily_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:DailyCore::DailyCore> $<TARGET_FILE_DIR:pipecat_daily_tests>
  )
endif()

include(GoogleTest)
gtest_discover_tests(pipecat_daily_tests)
