# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Version 1.0.0 is a rewrite for the Pipecat C++ Client SDK 1.0, which speaks
RTVI protocol 2.1 like current Pipecat bots. It's not compatible with earlier
versions, which were never tagged (0.x), see
[Migrating from 0.x](#migrating-from-0x).

### Added

- `pipecat::DailyTransport`, a transport for `pipecat::PipecatClient`, in
  `<pipecat/daily/transport.h>`. `DailyTransportOptions` sets the audio
  sample rate and channels.
- Connecting with the Daily room that start endpoints, like Pipecat Cloud,
  return (`dailyRoom` and `dailyToken`), or with `url` and `token`.
- Daily errors, and the call ending unexpectedly (e.g. when the room is
  closed), are reported to the client.
- An installable CMake package (`find_package(pipecat_daily)` and
  `pipecat::daily`), which also finds the Daily Core SDK.
- Text chat and voice chat (PortAudio) examples that work with a bot on your
  machine or on Pipecat Cloud.
- An API reference generated with Doxygen (`PIPECAT_DAILY_BUILD_DOCS`).
- Unit tests, and CI on Linux (`x86_64` and `aarch64`, GCC and Clang), macOS
  and Windows.

### Changed

- Daily Core C++ SDK 0.22.0.
- Everything is now in the `pipecat` namespace, and the header is
  `<pipecat/daily/transport.h>`.
- C++17 on every platform. MSVC used C++20.
- The library is built with RTTI (no more `-fno-rtti`).
- The Daily Core SDK can also be set with `DailyCore_ROOT`, besides the
  `DAILY_CORE_PATH` environment variable.

### Removed

- `rtvi::DailyVoiceClient` and `include/daily_rtvi.h`. Use a
  `pipecat::PipecatClient` with a `DailyTransport` instead.
- Daily Bots support. Start your bots with Pipecat Cloud or your own start
  endpoint.
- `cmake/FindDailyPipecat.cmake`, `cmake/FindPipecat.cmake`, and the
  `DAILY_PIPECAT_SDK_PATH` and `PIPECAT_SDK_PATH` environment variables.
- The Daily Bots examples and their Node.js server.

### Fixed

- Joining a room fails with a `TransportStartError` if Daily returns an
  error, e.g. because of a wrong token, instead of looking connected.
- Rooms that don't need a token can be joined without one.
- Destroying the transport now shuts down Daily Core, so a new client can use
  a new `DailyTransport`. Since Daily Core supports one at a time, a second
  `DailyTransport` in use at the same time fails to start with a
  `TransportStartError`, instead of breaking the first one.

### Migrating from 0.x

| 0.x | 1.0 |
| --- | --- |
| `#include "daily_rtvi.h"` | `#include <pipecat/daily/transport.h>` and `#include <pipecat/pipecat.h>` |
| `rtvi::DailyVoiceClient` | `pipecat::PipecatClient`, with a `pipecat::DailyTransport` in its options |
| `rtvi::DailyTransportParams` | `pipecat::DailyTransportOptions` |
| Daily Bots start URL and configuration | A start endpoint, like Pipecat Cloud, with `createDailyRoom` |
| `room_url` and `token` in the connection info | `url` (or `dailyRoom` or `room_url`) and `token` (or `dailyToken`) |
| `FindDailyPipecat.cmake`, `DAILY_PIPECAT_SDK_PATH` and `PIPECAT_SDK_PATH` | `find_package(pipecat_daily)` with `CMAKE_PREFIX_PATH` |
