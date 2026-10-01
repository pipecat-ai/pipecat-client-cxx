#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Finds speexdsp, for its resampler, and defines SpeexDSP::SpeexDSP.
# speexdsp doesn't install a CMake package, only a pkg-config file.

find_path(SpeexDSP_INCLUDE_DIR speex/speex_resampler.h)
find_library(SpeexDSP_LIBRARY NAMES speexdsp libspeexdsp)
mark_as_advanced(SpeexDSP_INCLUDE_DIR SpeexDSP_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpeexDSP
  REQUIRED_VARS SpeexDSP_LIBRARY SpeexDSP_INCLUDE_DIR
)

if(SpeexDSP_FOUND AND NOT TARGET SpeexDSP::SpeexDSP)
  add_library(SpeexDSP::SpeexDSP UNKNOWN IMPORTED)
  set_target_properties(SpeexDSP::SpeexDSP PROPERTIES
    IMPORTED_LOCATION "${SpeexDSP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SpeexDSP_INCLUDE_DIR}"
  )
endif()
