#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_daily VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

option(PIPECAT_DAILY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#
# Dependencies
#

# The Pipecat client might already be part of the build.
if(NOT TARGET pipecat::pipecat)
  find_package(pipecat 1.0 REQUIRED)
endif()
find_package(DailyCore REQUIRED)
find_package(Threads REQUIRED)

# Daily Core is built with this setting, so everything that links it must be
# too, including the tests.
if(MSVC)
  add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

#
# Library
#
add_library(pipecat_daily STATIC
  src/session.cpp
  src/transport.cpp
)
add_library(pipecat::daily ALIAS pipecat_daily)

# The library is built as strict C++17 so newer features don't creep in, and
# consumers only need C++17 or newer.
target_compile_features(pipecat_daily PUBLIC cxx_std_17)
set_target_properties(pipecat_daily PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX d
  # Installed as pipecat::daily, like the alias above.
  EXPORT_NAME daily
)

target_include_directories(pipecat_daily PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(pipecat_daily
  PUBLIC pipecat::pipecat
  PRIVATE DailyCore::DailyCore Threads::Threads
)

if(MSVC)
  target_compile_definitions(pipecat_daily PUBLIC _ITERATOR_DEBUG_LEVEL=0)
  target_compile_options(pipecat_daily PRIVATE
    /W4
    $<$<BOOL:${PIPECAT_DAILY_WARNINGS_AS_ERRORS}>:/WX>
  )
else()
  target_compile_options(pipecat_daily PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<BOOL:${PIPECAT_DAILY_WARNINGS_AS_ERRORS}>:-Werror>
  )
endif()

if(APPLE)
  set_target_properties(pipecat_daily PROPERTIES CXX_VISIBILITY_PRESET hidden)
endif()
