#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_daily_text_chat LANGUAGES CXX)

find_package(pipecat 1.0 REQUIRED COMPONENTS daily)

add_executable(text_chat main.cpp)
target_link_libraries(text_chat PRIVATE pipecat::daily)

# Daily Core is a shared library. On Windows, copy it next to the app so it
# runs from the build folder.
if(WIN32)
  add_custom_command(TARGET text_chat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:DailyCore::DailyCore> $<TARGET_FILE_DIR:text_chat>
  )
endif()
