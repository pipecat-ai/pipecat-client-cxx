#
# Copyright (c) 2024, Daily
#

cmake_minimum_required(VERSION 3.16)

project(daily_pipecat LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

set(DAILY_PIPECAT_SOURCES
  src/daily_transport.cpp
  src/daily_voice_client.cpp
)

set(DAILY_PIPECAT_HEADERS
  include/daily_rtvi.h
  include/daily_transport.h
  include/daily_voice_client.h
)

add_library(daily_pipecat STATIC ${DAILY_PIPECAT_HEADERS} ${DAILY_PIPECAT_SOURCES})

set_target_properties(daily_pipecat PROPERTIES
  OUTPUT_NAME $<IF:$<CONFIG:Debug>,daily_pipecatd,daily_pipecat>
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/$<CONFIG>"
)

find_package(DailyCore)

find_package(Pipecat)

#
# This project header directories.
#
target_include_directories(daily_pipecat
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${PIPECAT_INCLUDE_DIRS}
  ${DAILY_CORE_INCLUDE_DIRS}
)

#
# Specific headers, libraries and flags for each paltform.
#
if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_ITERATOR_DEBUG_LEVEL=0")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()
