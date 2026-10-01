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

# speexdsp, to convert the bot's audio to the app's sample rate. Prefer an
# installed one and fall back to downloading it. Only its resampler is needed,
# so a downloaded one is built into the transport, with its functions renamed
# so they can't clash with another copy in the app.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(SpeexDSP QUIET)
if(SpeexDSP_FOUND)
  set(PIPECAT_WEBSOCKET_SPEEXDSP_BUILT_IN OFF)
else()
  message(STATUS "speexdsp not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(speexdsp
    URL https://downloads.xiph.org/releases/speex/speexdsp-1.2.1.tar.gz
    URL_HASH SHA256=8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d
  )
  FetchContent_MakeAvailable(speexdsp)
  enable_language(C)
  set(PIPECAT_WEBSOCKET_SPEEXDSP_BUILT_IN ON)
endif()

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
  PRIVATE LibDataChannel::LibDataChannel
)

if(PIPECAT_WEBSOCKET_SPEEXDSP_BUILT_IN)
  set(_speexdsp_resampler ${speexdsp_SOURCE_DIR}/libspeexdsp/resample.c)
  target_sources(pipecat_websocket PRIVATE ${_speexdsp_resampler})
  target_include_directories(pipecat_websocket SYSTEM PRIVATE
    ${speexdsp_SOURCE_DIR}/include
    ${speexdsp_SOURCE_DIR}/include/speex
  )
  target_compile_definitions(pipecat_websocket PRIVATE
    OUTSIDE_SPEEX
    RANDOM_PREFIX=pipecat_speexdsp
    FLOATING_POINT
  )
  # It's not our code, so our warnings don't apply to it.
  set_source_files_properties(${_speexdsp_resampler} PROPERTIES
    COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
  )
else()
  target_link_libraries(pipecat_websocket PRIVATE SpeexDSP::SpeexDSP)
endif()

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
