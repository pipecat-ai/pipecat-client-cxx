#
# Copyright (c) 2024, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Generates pipecat.pc at install time, so the prefix given to
# `cmake --install --prefix` is used. The PIPECAT_PC_* variables are set by an
# install(CODE) call that runs before this script.

foreach(dir LIBDIR INCLUDEDIR)
  if(NOT IS_ABSOLUTE "${PIPECAT_PC_${dir}}")
    set(PIPECAT_PC_${dir} "\${prefix}/${PIPECAT_PC_${dir}}")
  endif()
endforeach()

set(PIPECAT_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")

configure_file("${PIPECAT_PC_TEMPLATE}" "${PIPECAT_PC_OUTPUT}" @ONLY)
