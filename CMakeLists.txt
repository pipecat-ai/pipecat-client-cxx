#
# Copyright (c) 2024, Daily
#

cmake_minimum_required(VERSION 3.16)

project(daily_bots LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DAILY_BOTS_SOURCES
  src/daily_bots_transport.cpp
  src/rtvi_client.cpp
  src/rtvi_utils.cpp
)

set(DAILY_BOTS_HEADERS
  include/json.hpp
  include/daily_bots.h
  include/daily_bots_transport.h
  include/rtvi.h
  include/rtvi_callbacks.h
  include/rtvi_client.h
  include/rtvi_exceptions.h
  include/rtvi_messages.h
  include/rtvi_transport.h
  include/rtvi_utils.h
)

#
# DAILY_CORE_PATH environment variable should be defined.
#
if (NOT DEFINED ENV{DAILY_CORE_PATH})
  message(FATAL_ERROR "You must define DAILY_CORE_PATH environment variable.")
endif()

set(DAILY_CORE_PATH "$ENV{DAILY_CORE_PATH}")

add_library(daily_bots STATIC ${DAILY_BOTS_HEADERS} ${DAILY_BOTS_SOURCES})

#
# Look for libdaily_core.a
#
find_library(DAILY_CORE daily_core HINTS "${DAILY_CORE_PATH}/lib")
if(NOT DAILY_CORE)
  message(FATAL_ERROR "Couldn't find 'daily_core' library.")
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
target_include_directories(daily_bots
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${DAILY_CORE_PATH}/include
  ${CURL_INCLUDE_DIRS}
)

#
# Specific headers, libraries and flags for each paltform.
#
if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()
