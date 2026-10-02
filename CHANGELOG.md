# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Version 1.0.0 is a rewrite that brings the SDK up to RTVI protocol 2.1, which
current Pipecat bots speak, and in line with the JavaScript client
(`@pipecat-ai/client-js`). It's not compatible with earlier versions, which
were never tagged (0.x), see [Migrating from 0.x](#migrating-from-0x).

The Daily transport is now part of this repository, see
[Daily transport](#daily-transport), and there's a new
[WebSocket transport](#websocket-transport).

### Added

- `pipecat::PipecatClient`, the new client:
  - `start_bot()` starts a bot through an endpoint (e.g. Pipecat Cloud or your
    own server), with a timeout.
  - `connect()` connects the transport and waits until the bot is ready, with
    a timeout. `start_bot_and_connect()` does both.
  - `disconnect()` cancels a `start_bot()` or `connect()` in progress, and is
    safe to call from callbacks.
  - `state()` and `on_transport_state_changed()` follow the connection through
    the same states as client-js.
- `PipecatClientCallbacks`, with typed callbacks for every current server
  message: bot output, transcriptions, interruptions, LLM and TTS events,
  metrics, function calls, search responses, user mute, server messages and
  errors.
  Callbacks run on the client's own event thread, one at a time and in order.
- Messages to the bot: `send_text()`, `send_client_message()`,
  `send_client_request()` (with a callback or a `std::future`, and a timeout),
  `disconnect_bot()` and `send_dtmf()`.
- Function calls the app runs: `on_llm_function_call_in_progress()` gets a
  `respond` callback to send the result, right away or later, from any
  thread.
- `Transport` and `TransportObserver`, to write transports.
- Errors like client-js's: `StartBotError`, `ConnectionTimeoutError`,
  `BotNotReadyError`, `MessageError`, `RequestTimeoutError`, ...
- `client-ready` now sends the protocol version and details about the library
  and platform (`PipecatClientOptions::about`).
- An installable CMake package (`find_package(pipecat)` and
  `pipecat::pipecat`), and a pkg-config file for Meson and others.
- `pipecat/version.h`, with `PIPECAT_VERSION`.
- An API reference generated with Doxygen (`PIPECAT_BUILD_DOCS`).
- Unit tests, and CI on Linux (`x86_64` and `aarch64`, GCC 9 and newer, Clang
  and ThreadSanitizer), macOS and Windows.

### Changed

- Everything is now in the `pipecat` namespace (`pipecat::rtvi` for protocol
  types), and the headers are in `include/pipecat/`.
- C++17 on every platform. MSVC used C++20.
- On Windows, `_ITERATOR_DEBUG_LEVEL` is no longer set to 0, so apps can use
  Debug builds with their usual settings.
- nlohmann/json (3.7 or newer) is now a dependency instead of a bundled 3.7.3
  copy. It's downloaded if it's not installed.
- The library is built with RTTI (no more `-fno-rtti`), so apps with RTTI can
  subclass its classes.

### Removed

- `rtvi::RTVIClient`, with its actions, service configuration and helpers
  (`send_action()`, `RTVIHelper`, `RTVILLMHelper`, ...). Pipecat bots no longer
  support them.
- `cmake/FindPipecat.cmake` and the `PIPECAT_SDK_PATH` environment variable.
- Deprecated RTVI messages: `bot-transcription` and `tts-text` (use
  `bot-output`), and `llm-function-call` (use `llm-function-call-in-progress`).

### Fixed

- Messages with missing or unexpected fields no longer crash the app. They're
  reported to `on_error()` instead.
- Calling `disconnect()` from a callback no longer deadlocks with transports
  that deliver events on a single thread, like Daily.
- Slow function calls no longer block other events.
- libcurl handles are no longer leaked when starting the bot fails.

### Daily transport

The Daily transport moved here from
[pipecat-client-cxx-daily](https://github.com/pipecat-ai/pipecat-client-cxx-daily),
and was rewritten for this version, like the client.

#### Added

- `pipecat::DailyTransport`, in `<pipecat/daily/transport.h>`, built with
  `-DPIPECAT_BUILD_DAILY=ON`. `DailyTransportOptions` sets the audio sample
  rate and channels.
- Connecting with the Daily room that start endpoints, like Pipecat Cloud,
  return (`dailyRoom` and `dailyToken`), or with `url` and `token`.
- Daily errors, and the call ending unexpectedly (e.g. when the room is
  closed), are reported to the client.
- Several clients with a `DailyTransport` can run at the same time, e.g. to
  talk to several bots.
- The `daily` component of the CMake package
  (`find_package(pipecat COMPONENTS daily)` and `pipecat::daily`), which also
  finds the Daily Core SDK.
- Text chat and voice chat (PortAudio) examples that work with a bot on your
  machine or on Pipecat Cloud.
- It's documented in the client's API reference, and tested in its CI.

#### Changed

- Daily Core C++ SDK 0.23.0 or newer. It's a shared library, so apps ship it
  with them (see the transport's README).
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

#### Removed

- `rtvi::DailyVoiceClient` and `include/daily_rtvi.h`. Use a
  `pipecat::PipecatClient` with a `DailyTransport` instead.
- Daily Bots support. Start your bots with Pipecat Cloud or your own start
  endpoint.
- `cmake/FindDailyPipecat.cmake` and the `DAILY_PIPECAT_SDK_PATH` environment
  variable.
- The Daily Bots examples and their Node.js server.

#### Fixed

- Joining a room fails with a `TransportStartError` if Daily returns an
  error, e.g. because of a wrong token, instead of looking connected.
- Rooms that don't need a token can be joined without one.
- A second transport no longer breaks the first one, and Daily Core is shut
  down after the last transport is destroyed.

### WebSocket transport

#### Added

- `pipecat::WebSocketTransport`, in `<pipecat/websocket/transport.h>`, built
  with `-DPIPECAT_BUILD_WEBSOCKET=ON`. It connects to bots that use Pipecat's
  `ProtobufFrameSerializer`, like the development runner's
  `python bot.py -t websocket`, with the `wsUrl` and `token` start endpoints
  return. `WebSocketTransportOptions` sets the audio sample rate and channels.
- The bot's audio is converted to the app's sample rate and channels with
  speexdsp, and dropped when the bot is interrupted.
- The `websocket` component of the CMake package
  (`find_package(pipecat COMPONENTS websocket)` and `pipecat::websocket`),
  which also finds libdatachannel and speexdsp. Both are downloaded if
  they're not installed.
- The examples can use it, with `--transport websocket`.

### Migrating from 0.x

| 0.x | 1.0 |
| --- | --- |
| `#include "rtvi.h"` | `#include <pipecat/pipecat.h>` |
| `rtvi::RTVIClient` | `pipecat::PipecatClient` |
| `RTVIClientOptions` and a transport argument | `PipecatClientOptions`, with the transport in it |
| `RTVIEventCallbacks` | `PipecatClientCallbacks` |
| `initialize()` and `connect()` to the endpoint in the options | `start_bot_and_connect(request)`, or `start_bot()` and `connect()` |
| `on_bot_transcript()` | `on_bot_output()` |
| `on_generic_message()` | `on_unhandled_message()` |
| `send_action()` and service configuration | `send_client_message()` or `send_client_request()`, handled by your bot |
| `RTVILLMHelper` and `on_function_call()` | `on_llm_function_call_in_progress()`, answered with its `respond` callback |
| `RTVITransport` | `Transport` and `TransportObserver` |
| `FindPipecat.cmake` and `PIPECAT_SDK_PATH` | `find_package(pipecat)` with `CMAKE_PREFIX_PATH`, or pkg-config |

For the Daily transport:

| 0.x | 1.0 |
| --- | --- |
| The `pipecat-client-cxx-daily` repository | `transports/daily` in this repository, built with `-DPIPECAT_BUILD_DAILY=ON` |
| `#include "daily_rtvi.h"` | `#include <pipecat/daily/transport.h>` and `#include <pipecat/pipecat.h>` |
| `rtvi::DailyVoiceClient` | `pipecat::PipecatClient`, with a `pipecat::DailyTransport` in its options |
| `rtvi::DailyTransportParams` | `pipecat::DailyTransportOptions` |
| Daily Bots start URL and configuration | A start endpoint, like Pipecat Cloud, with `createDailyRoom` |
| `room_url` and `token` in the connection info | `url` (or `dailyRoom` or `room_url`) and `token` (or `dailyToken`) |
| `FindDailyPipecat.cmake` and `DAILY_PIPECAT_SDK_PATH` | `find_package(pipecat COMPONENTS daily)` with `CMAKE_PREFIX_PATH` |
| `DAILY_CORE_PATH` | `DailyCore_ROOT` |
