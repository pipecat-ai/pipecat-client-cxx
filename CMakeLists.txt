#
# Copyright (c) 2024, Daily
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
# it's included with add_subdirectory() or FetchContent.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(PIPECAT_TOP_LEVEL ON)
else()
  set(PIPECAT_TOP_LEVEL OFF)
endif()

option(PIPECAT_BUILD_TESTS "Build the unit tests" ${PIPECAT_TOP_LEVEL})
option(PIPECAT_INSTALL "Generate the install target" ON)
option(PIPECAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(GNUInstallDirs)

#
# Dependencies
#
find_package(CURL REQUIRED)

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

# Kept from the original build. Consumers linking a Debug build must use the
# same value.
if(MSVC)
  add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

#
# Library
#
add_library(pipecat STATIC
  src/rtvi_client.cpp
  src/rtvi_llm_helper.cpp
  src/rtvi_utils.cpp
)
add_library(pipecat::pipecat ALIAS pipecat)

# The library is built as strict C++17 so newer features don't creep in, and
# consumers only need C++17 or newer.
target_compile_features(pipecat PUBLIC cxx_std_17)
set_target_properties(pipecat PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX d
)

target_include_directories(pipecat PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(pipecat
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE CURL::libcurl
)

if(MSVC)
  target_compile_options(pipecat PRIVATE
    /W4
    $<$<BOOL:${PIPECAT_WARNINGS_AS_ERRORS}>:/WX>
  )
else()
  target_compile_options(pipecat PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<BOOL:${PIPECAT_WARNINGS_AS_ERRORS}>:-Werror>
  )
endif()

if(APPLE)
  set_target_properties(pipecat PROPERTIES CXX_VISIBILITY_PRESET hidden)
endif()

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
