#
# Copyright (c) 2024, Daily
#

cmake_minimum_required(VERSION 3.16)

project(daily_bots_example LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DAILY_BOTS_EXAMPLE_SOURCES
  src/daily_bots_example.cpp
)

#
# DAILY_CORE_PATH and DAILY_BOTS_PATH environment variables should be defined.
#
if (NOT DEFINED ENV{DAILY_CORE_PATH})
  message(FATAL_ERROR "You must define DAILY_CORE_PATH environment variable.")
endif()

if (NOT DEFINED ENV{DAILY_BOTS_PATH})
  message(FATAL_ERROR "You must define DAILY_BOTS_PATH environment variable.")
endif()

set(DAILY_CORE_PATH "$ENV{DAILY_CORE_PATH}")
set(DAILY_BOTS_PATH "$ENV{DAILY_BOTS_PATH}")

add_executable(daily_bots_example ${DAILY_BOTS_EXAMPLE_SOURCES})

set_target_properties(daily_bots_example PROPERTIES OUTPUT_NAME "daily-bots-example")

#
# Look for libdaily_core.a
#
find_library(DAILY_CORE daily_core HINTS "${DAILY_CORE_PATH}/lib")
if(NOT DAILY_CORE)
  message(FATAL_ERROR "Couldn't find 'daily_core' library.")
endif()

#
# Look for libdaily_bots.a
#
find_library(DAILY_BOTS daily_bots HINTS "${DAILY_BOTS_PATH}/lib")
if(NOT DAILY_BOTS)
  message(FATAL_ERROR "Couldn't find 'daily_bots' library.")
endif()

#
# Look for libcurl
#
find_package(CURL REQUIRED)

#
# C++ and Linker flags.
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti")

#
# This project header directories.
#
target_include_directories(daily_bots_example
  PRIVATE
  ${DAILY_BOTS_PATH}/include
  ${DAILY_CORE_PATH}/include
)

target_link_libraries(daily_bots_example
  PRIVATE
  ${DAILY_BOTS}
  ${DAILY_CORE}
  CURL::libcurl
)

#
# Specific headers, libraries and flags for each paltform.
#
if(APPLE)
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

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

  target_link_libraries(daily_bots_example
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
