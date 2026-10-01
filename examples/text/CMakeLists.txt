#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_text_chat LANGUAGES CXX)

# The example can use the transports the Pipecat client was installed with,
# if their dependencies are found.
find_package(pipecat 1.0 REQUIRED OPTIONAL_COMPONENTS daily websocket)
if(NOT pipecat_daily_FOUND AND NOT pipecat_websocket_FOUND)
  message(FATAL_ERROR "No transport found:\n"
    "  ${pipecat_daily_NOT_FOUND_MESSAGE}\n"
    "  ${pipecat_websocket_NOT_FOUND_MESSAGE}"
  )
endif()

add_executable(text_chat main.cpp)
target_link_libraries(text_chat PRIVATE pipecat::pipecat)

if(pipecat_daily_FOUND)
  target_link_libraries(text_chat PRIVATE pipecat::daily)
  target_compile_definitions(text_chat PRIVATE PIPECAT_EXAMPLE_DAILY)
  # Daily Core is a shared library. On Windows, copy it next to the app so it
  # runs from the build folder.
  if(WIN32)
    add_custom_command(TARGET text_chat POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DailyCore::DailyCore> $<TARGET_FILE_DIR:text_chat>
    )
  endif()
endif()

if(pipecat_websocket_FOUND)
  target_link_libraries(text_chat PRIVATE pipecat::websocket)
  target_compile_definitions(text_chat PRIVATE PIPECAT_EXAMPLE_WEBSOCKET)
endif()
