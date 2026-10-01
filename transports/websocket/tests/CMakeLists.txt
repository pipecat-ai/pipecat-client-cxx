#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

add_executable(pipecat_websocket_tests
  test_frames.cpp
)

set_target_properties(pipecat_websocket_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Tests also cover private code in src/.
target_include_directories(pipecat_websocket_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(pipecat_websocket_tests PRIVATE
  pipecat::websocket
  GTest::gtest_main
)

gtest_discover_tests(pipecat_websocket_tests)
