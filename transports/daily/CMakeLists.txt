#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# The Daily transport, pipecat::daily. Built with -DPIPECAT_BUILD_DAILY=ON.

#
# Dependencies
#

# The oldest Daily Core SDK that works: the first with a CMake package, and
# with the custom audio track fixes the transport needs. Point DailyCore_ROOT
# (a CMake or environment variable) or CMAKE_PREFIX_PATH to the SDK.
set(PIPECAT_DAILY_MIN_DAILY_CORE_VERSION 0.23.0)
find_package(DailyCore ${PIPECAT_DAILY_MIN_DAILY_CORE_VERSION} CONFIG REQUIRED)

#
# Library
#
add_library(pipecat_daily STATIC
  src/context.cpp
  src/session.cpp
  src/transport.cpp
)
add_library(pipecat::daily ALIAS pipecat_daily)
pipecat_set_target_defaults(pipecat_daily)
# Installed as pipecat::daily, like the alias above.
set_target_properties(pipecat_daily PROPERTIES EXPORT_NAME daily)

target_include_directories(pipecat_daily
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  # The core's private code transports share, like AudioBuffer.
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pipecat_daily
  PUBLIC pipecat::pipecat
  PRIVATE DailyCore::DailyCore Threads::Threads
)

#
# Install, as the `daily` component of the pipecat package
#
if(PIPECAT_INSTALL)
  install(TARGETS pipecat_daily
    EXPORT pipecat-daily-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(EXPORT pipecat-daily-targets
    NAMESPACE pipecat::
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )

  configure_file(cmake/pipecat-daily.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pipecat-daily.cmake
    @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pipecat-daily.cmake
    DESTINATION ${PIPECAT_CMAKE_DIR}
  )
endif()

#
# Tests
#
if(PIPECAT_BUILD_TESTS)
  add_subdirectory(tests)
endif()
