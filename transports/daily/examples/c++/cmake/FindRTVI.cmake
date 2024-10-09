# FindRTVI.cmake
#
# This module defines:
#   RTVI_LIBRARIES
#   RTVI_INCLUDE_DIRS
#   RTVI_FOUND
#

# Check if the RTVI_SDK_PATH environment variable is set.
if (NOT DEFINED ENV{RTVI_SDK_PATH})
  message(FATAL_ERROR "You must define RTVI_SDK_PATH environment variable.")
endif()

set(RTVI_SDK_PATH "$ENV{RTVI_SDK_PATH}")

find_path(RTVI_INCLUDE_DIR
  NAMES rtvi.h
  PATHS ${RTVI_SDK_PATH}/include
)
mark_as_advanced(RTVI_INCLUDE_DIR)

find_library(RTVI_LIBRARY_RELEASE
  NAMES rtvi
  HINTS ${RTVI_SDK_PATH}/lib ${RTVI_SDK_PATH}/lib/Release
  PATH_SUFFIXES lib
)
mark_as_advanced(RTVI_LIBRARY_RELEASE)

find_library(RTVI_LIBRARY_DEBUG
  NAMES rtvid
  HINTS ${RTVI_SDK_PATH}/lib/Debug
  PATH_SUFFIXES lib
)
mark_as_advanced(RTVI_LIBRARY_DEBUG)

include(SelectLibraryConfigurations)
select_library_configurations(RTVI)

if(RTVI_LIBRARY AND RTVI_INCLUDE_DIR)
  set(RTVI_LIBRARIES "${RTVI_LIBRARY}")
  set(RTVI_INCLUDE_DIRS ${RTVI_INCLUDE_DIR})
  set(RTVI_FOUND TRUE)
else()
  set(RTVI_FOUND FALSE)
endif()

if(RTVI_FOUND)
  message(STATUS "Found RTVI: ${RTVI_LIBRARIES}")
else()
  message(STATUS "RTVI library not found")
endif()
