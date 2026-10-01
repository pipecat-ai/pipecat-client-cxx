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
- Several clients with a `DailyTransport` can run at the same time, e.g. to
  talk to several bots.
- An installable CMake package (`find_package(pipecat_daily)` and
  `pipecat::daily`), which also finds the Daily Core SDK.
- Text chat and voice chat (PortAudio) examples that work with a bot on your
  machine or on Pipecat Cloud.
- An API reference generated with Doxygen (`PIPECAT_DAILY_BUILD_DOCS`).
- Unit tests, and CI on Linux (`x86_64` and `aarch64`, GCC and Clang), macOS
  and Windows.

### Changed

- Daily Core C++ SDK 0.23.0 or newer. It's a shared library, so apps ship it
  with them (see the README).
- On Windows, `_ITERATOR_DEBUG_LEVEL` is no longer set to 0, so apps can use
  Debug builds with their usual settings.
- Everything is now in the `pipecat` namespace, and the header is
  `<pipecat/daily/transport.h>`.
- C++17 on every platform. MSVC used C++20.
- The library is built with RTTI (no more `-fno-rtti`).
- The Daily Core SDK is found through its CMake package: point
  `DailyCore_ROOT` or `CMAKE_PREFIX_PATH` to it, instead of setting
  `DAILY_CORE_PATH`.
- Daily Core is told the version of the SDK the transport is built with,
  instead of a fixed one.
- User audio is sent with a custom audio track, and bot audio is received from
  the bot's track, instead of through Daily Core's virtual microphone and
  speaker, which only one transport per app can use. User audio no longer
  goes through echo cancellation, so apps that play the bot through speakers
  need their platform's echo cancellation or headphones.

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
- A second transport no longer breaks the first one, and Daily Core is shut
  down after the last transport is destroyed.

### Migrating from 0.x

| 0.x | 1.0 |
| --- | --- |
| `#include "daily_rtvi.h"` | `#include <pipecat/daily/transport.h>` and `#include <pipecat/pipecat.h>` |
| `rtvi::DailyVoiceClient` | `pipecat::PipecatClient`, with a `pipecat::DailyTransport` in its options |
| `rtvi::DailyTransportParams` | `pipecat::DailyTransportOptions` |
| Daily Bots start URL and configuration | A start endpoint, like Pipecat Cloud, with `createDailyRoom` |
| `room_url` and `token` in the connection info | `url` (or `dailyRoom` or `room_url`) and `token` (or `dailyToken`) |
| `FindDailyPipecat.cmake`, `DAILY_PIPECAT_SDK_PATH` and `PIPECAT_SDK_PATH` | `find_package(pipecat_daily)` with `CMAKE_PREFIX_PATH` |
| `DAILY_CORE_PATH` | `DailyCore_ROOT` |
