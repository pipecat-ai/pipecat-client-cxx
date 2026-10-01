#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_voice_chat LANGUAGES CXX)

# The example can use the transports the Pipecat client was installed with,
# if their dependencies are found.
find_package(pipecat 1.0 REQUIRED OPTIONAL_COMPONENTS daily websocket)
if(NOT pipecat_daily_FOUND AND NOT pipecat_websocket_FOUND)
  message(FATAL_ERROR "No transport found:\n"
    "  ${pipecat_daily_NOT_FOUND_MESSAGE}\n"
    "  ${pipecat_websocket_NOT_FOUND_MESSAGE}"
  )
endif()

# PortAudio comes from vcpkg on Windows and has a pkg-config file elsewhere.
if(WIN32)
  find_package(portaudio CONFIG REQUIRED)
  if(TARGET portaudio)
    set(PORTAUDIO_TARGET portaudio)
  else()
    set(PORTAUDIO_TARGET portaudio_static)
  endif()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PortAudio REQUIRED IMPORTED_TARGET portaudio-2.0)
  set(PORTAUDIO_TARGET PkgConfig::PortAudio)
endif()

add_executable(voice_chat audio.cpp main.cpp)
target_link_libraries(voice_chat PRIVATE pipecat::pipecat ${PORTAUDIO_TARGET})

if(pipecat_daily_FOUND)
  target_link_libraries(voice_chat PRIVATE pipecat::daily)
  target_compile_definitions(voice_chat PRIVATE PIPECAT_EXAMPLE_DAILY)
  # Daily Core is a shared library. On Windows, copy it next to the app so it
  # runs from the build folder.
  if(WIN32)
    add_custom_command(TARGET voice_chat POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DailyCore::DailyCore> $<TARGET_FILE_DIR:voice_chat>
    )
  endif()
endif()

if(pipecat_websocket_FOUND)
  target_link_libraries(voice_chat PRIVATE pipecat::websocket)
  target_compile_definitions(voice_chat PRIVATE PIPECAT_EXAMPLE_WEBSOCKET)
endif()
