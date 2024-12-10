# FindPIPECAT.cmake
#
# This module defines:
#   PIPECAT_LIBRARIES
#   PIPECAT_INCLUDE_DIRS
#   PIPECAT_FOUND
#

# Check if the PIPECAT_SDK_PATH environment variable is set.
if (NOT DEFINED ENV{PIPECAT_SDK_PATH})
  message(FATAL_ERROR "You must define PIPECAT_SDK_PATH environment variable.")
endif()

set(PIPECAT_SDK_PATH "$ENV{PIPECAT_SDK_PATH}")

find_path(PIPECAT_INCLUDE_DIR
  NAMES rtvi.h
  PATHS ${PIPECAT_SDK_PATH}/include
)
mark_as_advanced(PIPECAT_INCLUDE_DIR)

find_library(PIPECAT_LIBRARY_RELEASE
  NAMES pipecat
  HINTS ${PIPECAT_SDK_PATH}/lib ${PIPECAT_SDK_PATH}/lib/Release
  PATH_SUFFIXES lib
)
mark_as_advanced(PIPECAT_LIBRARY_RELEASE)

find_library(PIPECAT_LIBRARY_DEBUG
  NAMES pipecatd
  HINTS ${PIPECAT_SDK_PATH}/lib/Debug
  PATH_SUFFIXES lib
)
mark_as_advanced(PIPECAT_LIBRARY_DEBUG)

include(SelectLibraryConfigurations)
select_library_configurations(PIPECAT)

if(PIPECAT_LIBRARY AND PIPECAT_INCLUDE_DIR)
  set(PIPECAT_LIBRARIES "${PIPECAT_LIBRARY}")
  set(PIPECAT_INCLUDE_DIRS ${PIPECAT_INCLUDE_DIR})
  set(PIPECAT_FOUND TRUE)
else()
  set(PIPECAT_FOUND FALSE)
endif()

if(PIPECAT_FOUND)
  message(STATUS "Found Pipecat: ${PIPECAT_LIBRARIES}")
else()
  message(STATUS "Pipecat library not found")
endif()
