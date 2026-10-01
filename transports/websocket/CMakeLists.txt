#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# The WebSocket transport, pipecat::websocket. Built with
# -DPIPECAT_BUILD_WEBSOCKET=ON.

#
# Dependencies
#

# libdatachannel, for its WebSocket client. Prefer an installed one (vcpkg,
# Homebrew...) and fall back to downloading it. A downloaded one is built as a
# static library, with OpenSSL, and installed with this transport.
set(PIPECAT_WEBSOCKET_MIN_LIBDATACHANNEL_VERSION 0.24)
find_package(LibDataChannel ${PIPECAT_WEBSOCKET_MIN_LIBDATACHANNEL_VERSION}
  CONFIG QUIET
)
if(NOT LibDataChannel_FOUND)
  message(STATUS "libdatachannel not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.24.6
    GIT_SHALLOW TRUE
    GIT_SUBMODULES deps/libjuice deps/plog deps/usrsctp
  )
  set(BUILD_SHARED_LIBS OFF)
  set(NO_EXAMPLES ON)
  set(NO_TESTS ON)
  # Only the WebSocket client is needed, not WebRTC media.
  set(NO_MEDIA ON)
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

# A static libdatachannel's package doesn't look for OpenSSL itself, so the
# installed websocket component does.
get_target_property(PIPECAT_WEBSOCKET_LIBDATACHANNEL_TYPE
  LibDataChannel::LibDataChannel TYPE
)

# speexdsp, to convert the bot's audio to the app's sample rate.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(SpeexDSP REQUIRED)

#
# Library
#
add_library(pipecat_websocket STATIC
  src/frames.cpp
  src/resampler.cpp
  src/transport.cpp
)
add_library(pipecat::websocket ALIAS pipecat_websocket)
pipecat_set_target_defaults(pipecat_websocket)
# Installed as pipecat::websocket, like the alias above.
set_target_properties(pipecat_websocket PROPERTIES EXPORT_NAME websocket)

target_include_directories(pipecat_websocket
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  # The core's private code transports share, like AudioBuffer.
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pipecat_websocket
  PUBLIC pipecat::pipecat
  PRIVATE LibDataChannel::LibDataChannel SpeexDSP::SpeexDSP
)

#
# Install, as the `websocket` component of the pipecat package
#
if(PIPECAT_INSTALL)
  install(TARGETS pipecat_websocket
    EXPORT pipecat-websocket-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(EXPORT pipecat-websocket-targets
    NAMESPACE pipecat::
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )

  configure_file(cmake/pipecat-websocket.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat-websocket.cmake
    @ONLY
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat-websocket.cmake
    cmake/FindSpeexDSP.cmake
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )
endif()

#
# Tests
#
if(PIPECAT_BUILD_TESTS)
  add_subdirectory(tests)
endif()
