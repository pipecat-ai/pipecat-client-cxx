# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Version 1.0.0 is a rewrite that brings the SDK up to RTVI protocol 2.1, which
current Pipecat bots speak, and in line with the JavaScript client
(`@pipecat-ai/client-js`). It's not compatible with earlier versions, which
were never tagged (0.x), see [Migrating from 0.x](#migrating-from-0x).

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
  message: bot output, transcriptions, LLM and TTS events, metrics,
  function calls, search responses, user mute, server messages and errors.
  Callbacks run on the client's own event thread, one at a time and in order.
- Messages to the bot: `send_text()`, `send_client_message()`,
  `send_client_request()` (with a callback or a `std::future`, and a timeout),
  `disconnect_bot()` and `send_dtmf()`.
- Function call handlers, with `register_function_call_handler()`. Handlers
  can respond right away or later, from any thread.
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
| `RTVILLMHelper` and `on_function_call()` | `register_function_call_handler()` |
| `RTVITransport` | `Transport` and `TransportObserver` |
| `FindPipecat.cmake` and `PIPECAT_SDK_PATH` | `find_package(pipecat)` with `CMAKE_PREFIX_PATH`, or pkg-config |
