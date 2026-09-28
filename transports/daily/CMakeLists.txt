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
option(PIPECAT_DAILY_BUILD_DOCS "Build the API reference with Doxygen" OFF)

include(GNUInstallDirs)

#
# Dependencies
#

# The Pipecat client might already be part of the build.
if(NOT TARGET pipecat::pipecat)
  find_package(pipecat 1.0 REQUIRED)
endif()
# The oldest Daily Core SDK that works: the first with a CMake package, and
# with the custom audio track fixes the transport needs. Point DailyCore_ROOT
# (a CMake or environment variable) or CMAKE_PREFIX_PATH to the SDK.
set(PIPECAT_DAILY_MIN_DAILY_CORE_VERSION 0.23.0)
find_package(DailyCore ${PIPECAT_DAILY_MIN_DAILY_CORE_VERSION} CONFIG REQUIRED)
find_package(Threads REQUIRED)

#
# Library
#
add_library(pipecat_daily STATIC
  src/audio_buffer.cpp
  src/context.cpp
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
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat_dailyConfigVersion.cmake
    DESTINATION ${PIPECAT_DAILY_CMAKE_DIR}
  )
endif()

#
# API reference
#
if(PIPECAT_DAILY_BUILD_DOCS)
  find_package(Doxygen REQUIRED)

  # doxygen-awesome-css theme, like the Pipecat C++ client's reference.
  set(PIPECAT_DAILY_DOXYGEN_THEME_URL
    https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.5.0
  )
  set(PIPECAT_DAILY_DOXYGEN_THEME_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/doxygen-awesome-css
  )
  file(DOWNLOAD
    ${PIPECAT_DAILY_DOXYGEN_THEME_URL}/doxygen-awesome.css
    ${PIPECAT_DAILY_DOXYGEN_THEME_DIR}/doxygen-awesome.css
    EXPECTED_HASH SHA256=ea37516ec7a7a7780719b9d9a285fe3268a8d5411523d8b93af98d77e67cd1a1
  )
  file(DOWNLOAD
    ${PIPECAT_DAILY_DOXYGEN_THEME_URL}/doxygen-awesome-sidebar-only.css
    ${PIPECAT_DAILY_DOXYGEN_THEME_DIR}/doxygen-awesome-sidebar-only.css
    EXPECTED_HASH SHA256=909d17055552149555fc87c5e0cf171a41d09a9e358c389a51b25ddfafd462af
  )

  set(DOXYGEN_PROJECT_NAME "Daily Transport for Pipecat C++ Client SDK")
  set(DOXYGEN_PROJECT_NUMBER ${PROJECT_VERSION})
  set(DOXYGEN_PROJECT_BRIEF "Connect to Pipecat bots over Daily")
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/docs/mainpage.md)
  set(DOXYGEN_STRIP_FROM_PATH ${PROJECT_SOURCE_DIR}/include)
  set(DOXYGEN_STRIP_FROM_INC_PATH ${PROJECT_SOURCE_DIR}/include)
  # Warn about anything public that isn't documented.
  set(DOXYGEN_EXTRACT_ALL NO)
  set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
  set(DOXYGEN_GENERATE_LATEX NO)
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  set(DOXYGEN_DISABLE_INDEX NO)
  set(DOXYGEN_FULL_SIDEBAR NO)
  set(DOXYGEN_HTML_COLORSTYLE LIGHT)
  set(DOXYGEN_HTML_EXTRA_STYLESHEET
    ${PIPECAT_DAILY_DOXYGEN_THEME_DIR}/doxygen-awesome.css
    ${PIPECAT_DAILY_DOXYGEN_THEME_DIR}/doxygen-awesome-sidebar-only.css
  )
  if(PIPECAT_DAILY_WARNINGS_AS_ERRORS)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
  endif()

  doxygen_add_docs(docs
    ${PROJECT_SOURCE_DIR}/docs/mainpage.md
    ${PROJECT_SOURCE_DIR}/include/pipecat/daily
    COMMENT "Generating the API reference"
  )
endif()

#
# Tests
#
if(PIPECAT_DAILY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
