#
# Copyright (c) 2024, Daily
#

cmake_minimum_required(VERSION 3.16)

project(pipecat LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

set(PIPECAT_SOURCES
  src/rtvi_client.cpp
  src/rtvi_llm_helper.cpp
  src/rtvi_utils.cpp
)

set(PIPECAT_HEADERS
  include/json.hpp
  include/rtvi.h
  include/rtvi_callbacks.h
  include/rtvi_client.h
  include/rtvi_exceptions.h
  include/rtvi_helper.h
  include/rtvi_llm_helper.h
  include/rtvi_messages.h
  include/rtvi_transport.h
  include/rtvi_utils.h
)

add_library(pipecat STATIC ${PIPECAT_HEADERS} ${PIPECAT_SOURCES})

set_target_properties(pipecat PROPERTIES
  OUTPUT_NAME $<IF:$<CONFIG:Debug>,pipecatd,pipecat>
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/$<CONFIG>"
)

#
# Look for libcurl
#
find_package(CURL REQUIRED)

#
# This project header directories.
#
target_include_directories(pipecat
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CURL_INCLUDE_DIRS}
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
