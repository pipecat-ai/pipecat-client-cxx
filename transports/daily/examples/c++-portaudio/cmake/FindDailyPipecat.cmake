# FindDailyPipecat.cmake
#
# This module defines:
#   DAILY_PIPECAT_LIBRARIES
#   DAILY_PIPECAT_INCLUDE_DIRS
#   DAILY_PIPECAT_FOUND
#

# Check if the DAILY_PIPECAT_SDK_PATH environment variable is set.
if (NOT DEFINED ENV{DAILY_PIPECAT_SDK_PATH})
  message(FATAL_ERROR "You must define DAILY_PIPECAT_SDK_PATH environment variable.")
endif()

set(DAILY_PIPECAT_SDK_PATH "$ENV{DAILY_PIPECAT_SDK_PATH}")

find_path(DAILY_PIPECAT_INCLUDE_DIR
  NAMES daily_rtvi.h
  PATHS ${DAILY_PIPECAT_SDK_PATH}/include
)
mark_as_advanced(DAILY_PIPECAT_INCLUDE_DIR)

find_library(DAILY_PIPECAT_LIBRARY_RELEASE
  NAMES daily_pipecat
  HINTS ${DAILY_PIPECAT_SDK_PATH}/lib ${DAILY_PIPECAT_SDK_PATH}/lib/Release
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_PIPECAT_LIBRARY_RELEASE)

find_library(DAILY_PIPECAT_LIBRARY_DEBUG
  NAMES daily_pipecatd
  HINTS ${DAILY_PIPECAT_SDK_PATH}/lib/Debug
  PATH_SUFFIXES lib
)
mark_as_advanced(DAILY_PIPECAT_LIBRARY_DEBUG)

include(SelectLibraryConfigurations)
select_library_configurations(DAILY_PIPECAT)

if(DAILY_PIPECAT_LIBRARY AND DAILY_PIPECAT_INCLUDE_DIR)
  set(DAILY_PIPECAT_LIBRARIES "${DAILY_PIPECAT_LIBRARY}")
  set(DAILY_PIPECAT_INCLUDE_DIRS ${DAILY_PIPECAT_INCLUDE_DIR})
  set(DAILY_PIPECAT_FOUND TRUE)
else()
  set(DAILY_PIPECAT_FOUND FALSE)
endif()

if(DAILY_PIPECAT_FOUND)
  message(STATUS "Found Daily Pipecat: ${DAILY_PIPECAT_LIBRARIES}")
else()
  message(STATUS "Daily Pipecat library not found")
endif()
