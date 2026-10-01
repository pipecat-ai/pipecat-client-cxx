#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

add_executable(pipecat_tests
  test_client.cpp
  test_event_loop.cpp
  test_messaging.cpp
  test_rtvi_messages.cpp
  test_start_bot.cpp
)

set_target_properties(pipecat_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Tests also cover private code in src/.
target_include_directories(pipecat_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(pipecat_tests PRIVATE
  pipecat::pipecat
  GTest::gtest_main
  Threads::Threads
)

gtest_discover_tests(pipecat_tests)
