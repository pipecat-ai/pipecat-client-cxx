# FindRTVI.cmake
#
# This module defines:
#   DAILY_RTVI_LIBRARIES
#   DAILY_RTVI_INCLUDE_DIRS
#   DAILY_RTVI_FOUND
#

# Check if the DAILY_RTVI_SDK_PATH environment variable is set.
if (NOT DEFINED ENV{DAILY_RTVI_SDK_PATH})
  message(FATAL_ERROR "You must define DAILY_RTVI_SDK_PATH environment variable.")
endif()

set(DAILY_RTVI_SDK_PATH "$ENV{DAILY_RTVI_SDK_PATH}")

find_path(DAILY_RTVI_INCLUDE_DIR
  NAMES daily_rtvi.h
  PATHS ${DAILY_RTVI_SDK_PATH}/include
)
mark_as_advanced(DAILY_RTVI_INCLUDE_DIR)

find_library(DAILY_RTVI_LIBRARY_RELEASE
  NAMES daily_rtvi
  HINTS ${DAILY_RTVI_SDK_PATH}/lib ${DAILY_RTVI_SDK_PATH}/lib/Release
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_RTVI_LIBRARY_RELEASE)

find_library(DAILY_RTVI_LIBRARY_DEBUG
  NAMES daily_rtvid
  HINTS ${DAILY_RTVI_SDK_PATH}/lib/Debug
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_RTVI_LIBRARY_DEBUG)

include(SelectLibraryConfigurations)
select_library_configurations(DAILY_RTVI)

if(DAILY_RTVI_LIBRARY AND DAILY_RTVI_INCLUDE_DIR)
  set(DAILY_RTVI_LIBRARIES "${DAILY_RTVI_LIBRARY}")
  set(DAILY_RTVI_INCLUDE_DIRS ${DAILY_RTVI_INCLUDE_DIR})
  set(DAILY_RTVI_FOUND TRUE)
else()
  set(DAILY_RTVI_FOUND FALSE)
endif()

if(DAILY_RTVI_FOUND)
  message(STATUS "Found Daily RTVI: ${DAILY_RTVI_LIBRARIES}")
else()
  message(STATUS "Daily RTVI library not found")
endif()
