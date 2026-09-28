#
# Copyright (c) 2024, Daily
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Builds a small program against an installed Pipecat package to check that
# the pkg-config file works.

project('pipecat_package_test', 'cpp', default_options: ['cpp_std=c++17'])

pipecat_dep = dependency('pipecat', version: '>= 1.0')

executable('package_test', 'main.cpp', dependencies: pipecat_dep)
