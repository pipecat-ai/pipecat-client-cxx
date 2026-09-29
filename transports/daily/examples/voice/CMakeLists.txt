#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_daily_voice_chat LANGUAGES CXX)

find_package(pipecat_daily 1.0 REQUIRED)

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
target_link_libraries(voice_chat PRIVATE pipecat::daily ${PORTAUDIO_TARGET})

# Daily Core is a shared library. On Windows, copy it next to the app so it
# runs from the build folder.
if(WIN32)
  add_custom_command(TARGET voice_chat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:DailyCore::DailyCore> $<TARGET_FILE_DIR:voice_chat>
  )
endif()
