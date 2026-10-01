#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Tests are only built by default when this is the top-level project, not when
# it's included with add_subdirectory() or FetchContent. They're also off when
# cross-compiling, since they can't run on this machine.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_CROSSCOMPILING)
  set(PIPECAT_BUILD_TESTS_DEFAULT ON)
else()
  set(PIPECAT_BUILD_TESTS_DEFAULT OFF)
endif()

option(PIPECAT_BUILD_TESTS "Build the unit tests" ${PIPECAT_BUILD_TESTS_DEFAULT})
option(PIPECAT_INSTALL "Generate the install target" ON)
option(PIPECAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PIPECAT_BUILD_DOCS "Build the API reference with Doxygen" OFF)

# Transports. Each needs its own dependencies, so they're only built when asked.
option(PIPECAT_BUILD_DAILY "Build the Daily transport" OFF)
option(PIPECAT_BUILD_WEBSOCKET "Build the WebSocket transport" OFF)

include(GNUInstallDirs)

#
# Dependencies
#
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Prefer an installed nlohmann_json (apt, Homebrew, vcpkg, Yocto...) and fall
# back to downloading it.
find_package(nlohmann_json 3.7 QUIET)
if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  )
  set(JSON_Install ${PIPECAT_INSTALL})
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Settings for every library here: strict C++17, warnings, and hidden symbols
# on Apple platforms.
function(pipecat_set_target_defaults target)
  # Built as strict C++17 so newer features don't creep in, and consumers only
  # need C++17 or newer.
  target_compile_features(${target} PUBLIC cxx_std_17)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    DEBUG_POSTFIX d
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4
      $<$<BOOL:${PIPECAT_WARNINGS_AS_ERRORS}>:/WX>
    )
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      $<$<BOOL:${PIPECAT_WARNINGS_AS_ERRORS}>:-Werror>
    )
  endif()

  if(APPLE)
    set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden)
  endif()
endfunction()

#
# Library
#
add_library(pipecat STATIC
  src/client.cpp
  src/event_loop.cpp
  src/http.cpp
  src/rtvi/about.cpp
  src/rtvi/messages.cpp
  src/transport.cpp
)
add_library(pipecat::pipecat ALIAS pipecat)
pipecat_set_target_defaults(pipecat)

configure_file(cmake/version.h.in include/pipecat/version.h @ONLY)

target_include_directories(pipecat PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(pipecat
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE CURL::libcurl Threads::Threads
)

#
# Install
#
if(PIPECAT_INSTALL)
  include(CMakePackageConfigHelpers)

  set(PIPECAT_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/pipecat)

  install(TARGETS pipecat
    EXPORT pipecatTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/pipecat/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pipecat
  )

  install(EXPORT pipecatTargets
    NAMESPACE pipecat::
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )

  configure_package_config_file(
    cmake/pipecatConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pipecatConfig.cmake
    INSTALL_DESTINATION ${PIPECAT_CMAKE_DIR}
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/pipecatConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pipecatConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pipecatConfigVersion.cmake
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )

  # pkg-config file (for Meson and others). It's generated at install time so
  # the prefix given to `cmake --install --prefix` is used.
  #
  # Dependencies only go in Requires if they have a pkg-config file: macOS
  # ships libcurl without one, and nlohmann_json only has one since 3.9. Without
  # it, nlohmann_json is either in a system include path or was downloaded and
  # installed next to our headers.
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PIPECAT_PC_CURL QUIET libcurl)
    pkg_check_modules(PIPECAT_PC_JSON QUIET nlohmann_json)
  endif()
  set(PIPECAT_PC_REQUIRES "")
  set(PIPECAT_PC_LIBS_EXTRA "")
  if(PIPECAT_PC_CURL_FOUND)
    list(APPEND PIPECAT_PC_REQUIRES "libcurl")
  else()
    set(PIPECAT_PC_LIBS_EXTRA " -lcurl")
  endif()
  if(PIPECAT_PC_JSON_FOUND)
    list(APPEND PIPECAT_PC_REQUIRES "nlohmann_json >= 3.7")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    string(APPEND PIPECAT_PC_LIBS_EXTRA " ${CMAKE_THREAD_LIBS_INIT}")
  endif()
  string(REPLACE ";" ", " PIPECAT_PC_REQUIRES "${PIPECAT_PC_REQUIRES}")

  install(CODE "
    set(PIPECAT_PC_TEMPLATE \"${CMAKE_CURRENT_SOURCE_DIR}/cmake/pipecat.pc.in\")
    set(PIPECAT_PC_OUTPUT \"${CMAKE_CURRENT_BINARY_DIR}/pipecat.pc\")
    set(PIPECAT_PC_VERSION \"${PROJECT_VERSION}\")
    set(PIPECAT_PC_LIBDIR \"${CMAKE_INSTALL_LIBDIR}\")
    set(PIPECAT_PC_INCLUDEDIR \"${CMAKE_INSTALL_INCLUDEDIR}\")
    set(PIPECAT_PC_LIBNAME \"$<TARGET_FILE_BASE_NAME:pipecat>\")
    set(PIPECAT_PC_REQUIRES \"${PIPECAT_PC_REQUIRES}\")
    set(PIPECAT_PC_LIBS_EXTRA \"${PIPECAT_PC_LIBS_EXTRA}\")
  ")
  install(SCRIPT cmake/pipecatPkgConfig.cmake)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pipecat.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )
endif()

#
# API reference
#
if(PIPECAT_BUILD_DOCS)
  find_package(Doxygen REQUIRED)

  # doxygen-awesome-css theme.
  set(PIPECAT_DOXYGEN_THEME_URL
    https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.5.0
  )
  set(PIPECAT_DOXYGEN_THEME_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen-awesome-css)
  file(DOWNLOAD
    ${PIPECAT_DOXYGEN_THEME_URL}/doxygen-awesome.css
    ${PIPECAT_DOXYGEN_THEME_DIR}/doxygen-awesome.css
    EXPECTED_HASH SHA256=ea37516ec7a7a7780719b9d9a285fe3268a8d5411523d8b93af98d77e67cd1a1
  )
  file(DOWNLOAD
    ${PIPECAT_DOXYGEN_THEME_URL}/doxygen-awesome-sidebar-only.css
    ${PIPECAT_DOXYGEN_THEME_DIR}/doxygen-awesome-sidebar-only.css
    EXPECTED_HASH SHA256=909d17055552149555fc87c5e0cf171a41d09a9e358c389a51b25ddfafd462af
  )

  set(DOXYGEN_PROJECT_NAME "Pipecat C++ Client SDK")
  set(DOXYGEN_PROJECT_NUMBER ${PROJECT_VERSION})
  set(DOXYGEN_PROJECT_BRIEF "Connect native apps to Pipecat bots")
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/docs/mainpage.md)
  set(DOXYGEN_STRIP_FROM_PATH
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/transports/daily/include
  )
  set(DOXYGEN_STRIP_FROM_INC_PATH ${DOXYGEN_STRIP_FROM_PATH})
  # Warn about anything public that isn't documented.
  set(DOXYGEN_EXTRACT_ALL NO)
  set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
  set(DOXYGEN_GENERATE_LATEX NO)
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  set(DOXYGEN_DISABLE_INDEX NO)
  set(DOXYGEN_FULL_SIDEBAR NO)
  set(DOXYGEN_HTML_COLORSTYLE LIGHT)
  set(DOXYGEN_HTML_EXTRA_STYLESHEET
    ${PIPECAT_DOXYGEN_THEME_DIR}/doxygen-awesome.css
    ${PIPECAT_DOXYGEN_THEME_DIR}/doxygen-awesome-sidebar-only.css
  )
  if(PIPECAT_WARNINGS_AS_ERRORS)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
  endif()

  # The transports are always documented. Doxygen only reads their headers, so
  # it doesn't need their dependencies.
  doxygen_add_docs(docs
    ${PROJECT_SOURCE_DIR}/docs/mainpage.md
    ${PROJECT_SOURCE_DIR}/include/pipecat
    ${CMAKE_CURRENT_BINARY_DIR}/include/pipecat/version.h
    ${PROJECT_SOURCE_DIR}/transports/daily/docs/daily.md
    ${PROJECT_SOURCE_DIR}/transports/daily/include/pipecat/daily
    COMMENT "Generating the API reference"
  )
endif()

#
# Tests
#
if(PIPECAT_BUILD_TESTS)
  enable_testing()

  # Prefer an installed GoogleTest and fall back to downloading it.
  find_package(GTest 1.10 CONFIG QUIET)
  if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found, downloading it")
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
      URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    )
    set(gtest_force_shared_crt ON)
    set(INSTALL_GTEST OFF)
    set(BUILD_GMOCK OFF)
    FetchContent_MakeAvailable(googletest)
  endif()
  include(GoogleTest)

  add_subdirectory(tests)
endif()

#
# Transports
#
if(PIPECAT_BUILD_DAILY)
  add_subdirectory(transports/daily)
endif()
if(PIPECAT_BUILD_WEBSOCKET)
  add_subdirectory(transports/websocket)
endif()
