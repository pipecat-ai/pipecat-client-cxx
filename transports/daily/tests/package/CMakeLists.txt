#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Builds a small program against an installed Pipecat package with the Daily
# transport, to check that find_package(pipecat COMPONENTS daily) and the
# exported targets work.

cmake_minimum_required(VERSION 3.16)

project(pipecat_daily_package_test LANGUAGES CXX)

find_package(pipecat 1.0 REQUIRED COMPONENTS daily)

add_executable(package_test main.cpp)
target_link_libraries(package_test PRIVATE pipecat::daily)
