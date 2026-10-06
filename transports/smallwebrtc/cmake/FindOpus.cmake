#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Finds libopus, and defines Opus::opus. Distributions usually only install a
# pkg-config file for it, not a CMake package. Its header is included as
# <opus.h>, like its own package does.

find_path(Opus_INCLUDE_DIR opus.h PATH_SUFFIXES opus)
find_library(Opus_LIBRARY NAMES opus libopus)
mark_as_advanced(Opus_INCLUDE_DIR Opus_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
  REQUIRED_VARS Opus_LIBRARY Opus_INCLUDE_DIR
)

if(Opus_FOUND AND NOT TARGET Opus::opus)
  add_library(Opus::opus UNKNOWN IMPORTED)
  set_target_properties(Opus::opus PROPERTIES
    IMPORTED_LOCATION "${Opus_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Opus_INCLUDE_DIR}"
  )
  # A static libopus needs the math library.
  if(UNIX)
    set_property(TARGET Opus::opus PROPERTY INTERFACE_LINK_LIBRARIES m)
  endif()
endif()
