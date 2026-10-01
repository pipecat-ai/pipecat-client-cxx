#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

add_executable(pipecat_daily_tests
  test_session.cpp
  test_transport.cpp
)

set_target_properties(pipecat_daily_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Tests also cover private code in src/.
target_include_directories(pipecat_daily_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

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

gtest_discover_tests(pipecat_daily_tests)
