#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

add_executable(pipecat_smallwebrtc_tests
  test_params.cpp
)

set_target_properties(pipecat_smallwebrtc_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Tests also cover private code in src/.
target_include_directories(pipecat_smallwebrtc_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(pipecat_smallwebrtc_tests PRIVATE
  pipecat::smallwebrtc
  LibDataChannel::LibDataChannel
  GTest::gtest_main
)

gtest_discover_tests(pipecat_smallwebrtc_tests)
