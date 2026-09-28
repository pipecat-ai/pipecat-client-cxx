#
# Copyright (c) 2024-2026, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16)

project(pipecat_daily_text_chat LANGUAGES CXX)

find_package(pipecat_daily 1.0 REQUIRED)

add_executable(text_chat main.cpp)
target_link_libraries(text_chat PRIVATE pipecat::daily)
