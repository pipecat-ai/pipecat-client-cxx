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
  find_package(Git REQUIRED)
  FetchContent_Declare(libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.24.6
    GIT_SHALLOW TRUE
    GIT_SUBMODULES deps/libjuice deps/libsrtp deps/plog deps/usrsctp
    # So closing tells the bot right away, instead of it noticing about 30
    # seconds later.
    PATCH_COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace
      ${CMAKE_CURRENT_SOURCE_DIR}/patches/libdatachannel-dtls-close-notify.patch
    # The tag never changes, and updating would patch it again.
    UPDATE_DISCONNECTED TRUE
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

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# libopus, for the audio. Prefer an installed one and fall back to downloading
# it. A downloaded one is built as a static library, and installed with this
# transport.
find_package(Opus QUIET)
if(NOT Opus_FOUND)
  message(STATUS "libopus not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(opus
    URL https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
    URL_HASH SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
  )
  set(BUILD_SHARED_LIBS OFF)
  set(OPUS_BUILD_PROGRAMS OFF)
  set(OPUS_BUILD_TESTING OFF)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  FetchContent_MakeAvailable(opus)
  # It's not our code, so our warnings don't apply to it.
  target_compile_options(opus PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
endif()

# speexdsp, to convert audio between the app's sample rates and the ones Opus
# supports. Prefer an installed one and fall back to downloading it. Only its
# resampler is needed, so a downloaded one is built into the transport, with
# its functions renamed so they can't clash with another copy in the app.
find_package(SpeexDSP QUIET)
if(SpeexDSP_FOUND)
  set(PIPECAT_SMALLWEBRTC_SPEEXDSP_BUILT_IN OFF)
else()
  message(STATUS "speexdsp not found, downloading it")
  include(FetchContent)
  FetchContent_Declare(speexdsp
    URL https://downloads.xiph.org/releases/speex/speexdsp-1.2.1.tar.gz
    URL_HASH SHA256=8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d
  )
  FetchContent_MakeAvailable(speexdsp)
  enable_language(C)
  set(PIPECAT_SMALLWEBRTC_SPEEXDSP_BUILT_IN ON)
endif()

#
# Library
#
add_library(pipecat_smallwebrtc STATIC
  src/audio.cpp
  src/params.cpp
  src/resampler.cpp
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
  PRIVATE LibDataChannel::LibDataChannel Opus::opus
)

if(PIPECAT_SMALLWEBRTC_SPEEXDSP_BUILT_IN)
  set(_speexdsp_resampler ${speexdsp_SOURCE_DIR}/libspeexdsp/resample.c)
  target_sources(pipecat_smallwebrtc PRIVATE ${_speexdsp_resampler})
  target_include_directories(pipecat_smallwebrtc SYSTEM PRIVATE
    ${speexdsp_SOURCE_DIR}/include
    ${speexdsp_SOURCE_DIR}/include/speex
  )
  target_compile_definitions(pipecat_smallwebrtc PRIVATE
    OUTSIDE_SPEEX
    RANDOM_PREFIX=pipecat_smallwebrtc_speexdsp
    FLOATING_POINT
  )
  # It's not our code, so our warnings don't apply to it.
  set_source_files_properties(${_speexdsp_resampler} PROPERTIES
    COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
  )
else()
  target_link_libraries(pipecat_smallwebrtc PRIVATE SpeexDSP::SpeexDSP)
endif()

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
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat-smallwebrtc.cmake
    cmake/FindOpus.cmake
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
