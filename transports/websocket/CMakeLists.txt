#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# The WebSocket transport, pipecat::websocket. Built with
# -DPIPECAT_BUILD_WEBSOCKET=ON.

#
# Library
#
add_library(pipecat_websocket STATIC
  src/frames.cpp
)
add_library(pipecat::websocket ALIAS pipecat_websocket)
pipecat_set_target_defaults(pipecat_websocket)
# Installed as pipecat::websocket, like the alias above.
set_target_properties(pipecat_websocket PROPERTIES EXPORT_NAME websocket)

target_link_libraries(pipecat_websocket PUBLIC pipecat::pipecat)

#
# Tests
#
if(PIPECAT_BUILD_TESTS)
  add_subdirectory(tests)
endif()
