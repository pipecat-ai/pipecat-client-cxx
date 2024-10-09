# FindDailyCore.cmake
#
# This module defines:
#   DAILY_CORE_LIBRARIES
#   DAILY_CORE_INCLUDE_DIRS
#   DAILY_CORE_FOUND
#

# Check if the DAILY_CORE_PATH environment variable is set.
if (NOT DEFINED ENV{DAILY_CORE_PATH})
  message(FATAL_ERROR "You must define DAILY_CORE_PATH environment variable.")
endif()

set(DAILY_CORE_PATH "$ENV{DAILY_CORE_PATH}")

find_path(DAILY_CORE_INCLUDE_DIR
  NAMES daily_core.h
  PATHS ${DAILY_CORE_PATH}/include
)
mark_as_advanced(DAILY_CORE_INCLUDE_DIR)

find_library(DAILY_CORE_LIBRARY_RELEASE
  NAMES daily_core
  HINTS ${DAILY_CORE_PATH}/lib ${DAILY_CORE_PATH}/lib/Release
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_CORE_LIBRARY_RELEASE)

find_library(DAILY_CORE_LIBRARY_DEBUG
  NAMES daily_cored
  HINTS ${DAILY_CORE_PATH}/lib/Debug
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_CORE_LIBRARY_DEBUG)

include(SelectLibraryConfigurations)
select_library_configurations(DAILY_CORE)

if(DAILY_CORE_LIBRARY AND DAILY_CORE_INCLUDE_DIR)
  set(DAILY_CORE_LIBRARIES "${DAILY_CORE_LIBRARY}")
  set(DAILY_CORE_INCLUDE_DIRS ${DAILY_CORE_INCLUDE_DIR})
  set(DAILY_CORE_FOUND TRUE)
else()
  set(DAILY_CORE_FOUND FALSE)
endif()

if(DAILY_CORE_FOUND)
  message(STATUS "Found Daily Core: ${DAILY_CORE_LIBRARIES}")
else()
  message(STATUS "Daily Core library not found")
endif()
