#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Finds the Daily Core C++ SDK (https://github.com/daily-co/daily-core-sdk).
#
# Point DailyCore_ROOT or the DAILY_CORE_PATH environment variable to the
# unpacked SDK. Defines the DailyCore::DailyCore target, which also links the
# system libraries Daily Core needs.

find_path(DailyCore_INCLUDE_DIR
  NAMES daily_core.h
  HINTS ENV DAILY_CORE_PATH
  PATH_SUFFIXES include
)

find_library(DailyCore_LIBRARY_RELEASE
  NAMES daily_core
  HINTS ENV DAILY_CORE_PATH
  PATH_SUFFIXES lib lib/Release
)

find_library(DailyCore_LIBRARY_DEBUG
  NAMES daily_cored
  HINTS ENV DAILY_CORE_PATH
  PATH_SUFFIXES lib/Debug
)

mark_as_advanced(
  DailyCore_INCLUDE_DIR
  DailyCore_LIBRARY_RELEASE
  DailyCore_LIBRARY_DEBUG
)

include(SelectLibraryConfigurations)
select_library_configurations(DailyCore)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DailyCore
  REQUIRED_VARS DailyCore_LIBRARY DailyCore_INCLUDE_DIR
)

if(DailyCore_FOUND AND NOT TARGET DailyCore::DailyCore)
  add_library(DailyCore::DailyCore STATIC IMPORTED)
  set_target_properties(DailyCore::DailyCore PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${DailyCore_INCLUDE_DIR}"
  )

  if(DailyCore_LIBRARY_RELEASE)
    set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
    set_target_properties(DailyCore::DailyCore PROPERTIES
      IMPORTED_LOCATION "${DailyCore_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_RELEASE "${DailyCore_LIBRARY_RELEASE}"
    )
  endif()

  if(DailyCore_LIBRARY_DEBUG)
    set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG
    )
    set_target_properties(DailyCore::DailyCore PROPERTIES
      IMPORTED_LOCATION_DEBUG "${DailyCore_LIBRARY_DEBUG}"
    )
    if(NOT DailyCore_LIBRARY_RELEASE)
      set_target_properties(DailyCore::DailyCore PROPERTIES
        IMPORTED_LOCATION "${DailyCore_LIBRARY_DEBUG}"
      )
    endif()
  endif()

  # System libraries Daily Core needs.
  if(APPLE)
    foreach(framework
        AppKit AudioToolbox AVFoundation CoreAudio CoreGraphics CoreMedia
        CoreVideo Foundation IOSurface Metal MetalKit OpenGL QuartzCore
        ScreenCaptureKit Security VideoToolbox)
      find_library(DailyCore_${framework}_FRAMEWORK ${framework})
      mark_as_advanced(DailyCore_${framework}_FRAMEWORK)
      set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "${DailyCore_${framework}_FRAMEWORK}"
      )
    endforeach()
    set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
      INTERFACE_LINK_OPTIONS -ObjC
    )
  elseif(WIN32)
    set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES
        bcrypt crypt32 d3d11 dmoguids dwmapi dxgi gdi32 iphlpapi msdmo ncrypt
        ntdll ole32 secur32 shcore strmiids userenv winmm wmcodecdspuuid ws2_32
    )
  else()
    find_package(Threads REQUIRED)
    set_property(TARGET DailyCore::DailyCore APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS} m
    )
  endif()
endif()
