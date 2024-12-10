#
# Copyright (c) 2024, Daily
#

cmake_minimum_required(VERSION 3.16)

project(example_audio LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXAMPLE_AUDIO_HEADERS
  include/audio_device.h
)

set(EXAMPLE_AUDIO_SOURCES
  src/audio_device.cpp
  src/example_audio.cpp
)

add_executable(example_audio ${EXAMPLE_AUDIO_SOURCES} ${EXAMPLE_AUDIO_HEADERS})

find_package(DailyCore)

find_package(Pipecat)

find_package(DailyPipecat)

#
# Look for libcurl
#
find_package(CURL REQUIRED)

#
# Look for PortAudio
#
if(MSVC)
  find_package(portaudio REQUIRED)
else()
  pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
endif()

if(MSVC)
  target_link_libraries(example_audio
    PRIVATE
    portaudio
  )
else()
  target_include_directories(example_audio
    PRIVATE
    ${PORTAUDIO_INCLUDE_DIRS}
  )
  target_link_libraries(example_audio
    PRIVATE
    ${PORTAUDIO_LINK_LIBRARIES}
  )
endif()

#
# This project header directories.
#
target_include_directories(example_audio
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${DAILY_PIPECAT_INCLUDE_DIRS}
  ${PIPECAT_INCLUDE_DIRS}
  ${DAILY_CORE_INCLUDE_DIRS}
)

target_link_libraries(example_audio
  PRIVATE
  ${DAILY_PIPECAT_LIBRARIES}
  ${PIPECAT_LIBRARIES}
  ${DAILY_CORE_LIBRARIES}
  CURL::libcurl
)

#
# Specific headers, libraries and flags for each paltform.
#
if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

  find_library(CORE_GRAPHICS CoreGraphics)
  find_library(CORE_MEDIA CoreMedia)
  find_library(CORE_AUDIO CoreAudio)
  find_library(CORE_VIDEO CoreVideo)
  find_library(AUDIO_TOOLBOX AudioToolbox)
  find_library(VIDEO_TOOLBOX VideoToolbox)
  find_library(SECURITY Security)
  find_library(FOUNDATION Foundation)
  # The ones below are needed when linking with -ObjC
  find_library(APP_KIT AppKit)
  find_library(AVFOUNDATION AVFoundation)
  find_library(METAL Metal)
  find_library(METAL_KIT MetalKit)
  find_library(OPENGL OpenGL)
  find_library(QUARTZ_CORE QuartzCore)

  target_link_libraries(example_audio
    PRIVATE
    ${CORE_GRAPHICS}
    ${CORE_MEDIA}
    ${CORE_AUDIO}
    ${CORE_VIDEO}
    ${AUDIO_TOOLBOX}
    ${VIDEO_TOOLBOX}
    ${SECURITY}
    ${FOUNDATION}
    # The ones below are needed when linking with -ObjC
    -ObjC
    ${APP_KIT}
    ${AVFOUNDATION}
    ${METAL}
    ${METAL_KIT}
    ${OPENGL}
    ${QUARTZ_CORE}
  )
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_ITERATOR_DEBUG_LEVEL=0")

  # System libraries.
  target_link_libraries(example_audio
    PRIVATE
    msdmo
    wmcodecdspuuid
    dmoguids
    iphlpapi
    ole32
    secur32
    winmm
    ws2_32
    strmiids
    d3d11
    gdi32
    dxgi
    dwmapi
    shcore
    ntdll
    userenv
    bcrypt
  )
endif()
