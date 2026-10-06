#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# The SmallWebRTC transport, pipecat::smallwebrtc. Built with
# -DPIPECAT_BUILD_SMALLWEBRTC=ON.

#
# Dependencies
#

# libdatachannel, for WebRTC, built with media support. Prefer an installed
# one (vcpkg...) and fall back to downloading it. A downloaded one is built as
# a static library, with OpenSSL, and installed with this transport.
#
# The WebSocket transport uses libdatachannel too, and a build can only have
# one. When both are built, this one is set up first, so a downloaded one has
# media support, which the WebSocket transport doesn't mind. Keep both at the
# same version.
set(PIPECAT_SMALLWEBRTC_MIN_LIBDATACHANNEL_VERSION 0.24)
find_package(LibDataChannel ${PIPECAT_SMALLWEBRTC_MIN_LIBDATACHANNEL_VERSION}
  CONFIG QUIET
)
if(NOT LibDataChannel_FOUND)
  message(STATUS "libdatachannel not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.24.6
    GIT_SHALLOW TRUE
    GIT_SUBMODULES deps/libjuice deps/libsrtp deps/plog deps/usrsctp
  )
  set(BUILD_SHARED_LIBS OFF)
  set(NO_EXAMPLES ON)
  set(NO_TESTS ON)
  set(NO_MEDIA OFF)
  # libsrtp, for media, treats its warnings as errors, and they're not ours
  # to fix. Its tests aren't ours to run either.
  set(ENABLE_WARNINGS_AS_ERRORS OFF)
  set(LIBSRTP_TEST_APPS OFF)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  FetchContent_MakeAvailable(libdatachannel)

  # Its headers are system headers for us, like an installed one's, so our
  # warnings don't apply to them.
  get_target_property(_libdatachannel LibDataChannel::LibDataChannel
    ALIASED_TARGET
  )
  get_target_property(_libdatachannel_includes ${_libdatachannel}
    INTERFACE_INCLUDE_DIRECTORIES
  )
  set_target_properties(${_libdatachannel} PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_libdatachannel_includes}"
  )
endif()

get_target_property(_libdatachannel_definitions LibDataChannel::LibDataChannel
  INTERFACE_COMPILE_DEFINITIONS
)
if(NOT "RTC_ENABLE_MEDIA=1" IN_LIST _libdatachannel_definitions)
  message(FATAL_ERROR
    "The SmallWebRTC transport needs libdatachannel with media support, but "
    "the one found was built without it (NO_MEDIA)"
  )
endif()

# A static libdatachannel's package doesn't look for OpenSSL itself, so the
# installed smallwebrtc component does.
get_target_property(PIPECAT_SMALLWEBRTC_LIBDATACHANNEL_TYPE
  LibDataChannel::LibDataChannel TYPE
)

#
# Library
#
add_library(pipecat_smallwebrtc STATIC
  src/params.cpp
  src/transport.cpp
)
add_library(pipecat::smallwebrtc ALIAS pipecat_smallwebrtc)
pipecat_set_target_defaults(pipecat_smallwebrtc)
# Installed as pipecat::smallwebrtc, like the alias above.
set_target_properties(pipecat_smallwebrtc PROPERTIES EXPORT_NAME smallwebrtc)

target_include_directories(pipecat_smallwebrtc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  # The core's private code transports share, like its HTTP client.
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pipecat_smallwebrtc
  PUBLIC pipecat::pipecat
  PRIVATE LibDataChannel::LibDataChannel
)

#
# Install, as the `smallwebrtc` component of the pipecat package
#
if(PIPECAT_INSTALL)
  install(TARGETS pipecat_smallwebrtc
    EXPORT pipecat-smallwebrtc-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(EXPORT pipecat-smallwebrtc-targets
    NAMESPACE pipecat::
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )

  configure_file(cmake/pipecat-smallwebrtc.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat-smallwebrtc.cmake
    @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pipecat-smallwebrtc.cmake
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )
endif()

#
# Tests
#
if(PIPECAT_BUILD_TESTS)
  add_subdirectory(tests)
endif()
