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

# Tests are only built by default when this is the top-level project, not when
# it's included with add_subdirectory() or FetchContent. They're also off when
# cross-compiling, since they can't run on this machine.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_CROSSCOMPILING)
  set(PIPECAT_DAILY_BUILD_TESTS_DEFAULT ON)
else()
  set(PIPECAT_DAILY_BUILD_TESTS_DEFAULT OFF)
endif()

option(PIPECAT_DAILY_BUILD_TESTS "Build the unit tests" ${PIPECAT_DAILY_BUILD_TESTS_DEFAULT})
option(PIPECAT_DAILY_INSTALL "Generate the install target" ON)
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

#
# Install
#
if(PIPECAT_DAILY_INSTALL)
  include(CMakePackageConfigHelpers)

  set(PIPECAT_DAILY_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/pipecat_daily)

  install(TARGETS pipecat_daily
    EXPORT pipecat_dailyTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(EXPORT pipecat_dailyTargets
    NAMESPACE pipecat::
    DESTINATION ${PIPECAT_DAILY_CMAKE_DIR}
  )

  configure_package_config_file(
    cmake/pipecat_dailyConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfig.cmake
    INSTALL_DESTINATION ${PIPECAT_DAILY_CMAKE_DIR}
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
  )
  # Daily Core has no CMake package, so ship the module that finds it.
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfigVersion.cmake
    cmake/FindDailyCore.cmake
    DESTINATION ${PIPECAT_DAILY_CMAKE_DIR}
  )
endif()

#
# Tests
#
if(PIPECAT_DAILY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
