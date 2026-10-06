# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `pipecat::SmallWebRTCTransport`, a new transport, in
  `<pipecat/smallwebrtc/transport.h>`, built with
  `-DPIPECAT_BUILD_SMALLWEBRTC=ON`. It connects to bots that use Pipecat's
  SmallWebRTC transport, like the development runner's
  `python bot.py -t webrtc`, over WebRTC, peer to peer:
  - After `start_bot()`, it sends the WebRTC offer to the server that started
    the bot, like client-js. Apps can also give the offer endpoint in
    `webrtcRequestParams`, and STUN and TURN servers in `iceConfig`.
  - `SmallWebRTCTransportOptions` sets the audio sample rate and channels, at
    any sample rate. The audio is sent with Opus, in mono.
  - The bot's audio plays at the pace the bot sent it, 80 ms behind, so the
    network's ups and downs don't reach the app: audio that arrives late or
    out of order still plays in its turn, and lost audio is concealed.
  - The `smallwebrtc` component of the CMake package
    (`find_package(pipecat COMPONENTS smallwebrtc)` and
    `pipecat::smallwebrtc`), which also finds libdatachannel, libopus and
    speexdsp. They're downloaded if they're not installed, and a downloaded
    libdatachannel is patched so the bot notices right away when the client
    disconnects.
  - The examples can use it, with `--transport smallwebrtc`.

- `Transport::set_start_bot_params()` receives the request
  `PipecatClient::start_bot()` sends, so transports that connect through the
  server that started the bot can find where to connect. It does nothing by
  default, so existing transports don't need to change.

## [1.0.0] - 2026-10-02

### Added

- Version 1.0.0 is a rewrite that brings the SDK up to RTVI protocol 2.1, which
  current Pipecat bots speak, and in line with the JavaScript client
  (`@pipecat-ai/client-js`). It's not compatible with earlier versions, which
  were never tagged (0.x), see
  [Migrating from 0.x](https://github.com/pipecat-ai/pipecat-client-cxx/blob/v1.0.0/README.md#-migrating-from-0x).

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

- `pipecat::DailyTransport`, the Daily transport, which moved here from
  [pipecat-client-cxx-daily](https://github.com/pipecat-ai/pipecat-client-cxx-daily),
  and was rewritten for this version, like the client. It's in
  `<pipecat/daily/transport.h>`, built with `-DPIPECAT_BUILD_DAILY=ON`:
  - `DailyTransportOptions` sets the audio sample rate and channels.
  - It connects with the Daily room that start endpoints, like Pipecat Cloud,
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

- `pipecat::WebSocketTransport`, a new transport, in
  `<pipecat/websocket/transport.h>`, built with `-DPIPECAT_BUILD_WEBSOCKET=ON`.
  It connects to bots that use Pipecat's `ProtobufFrameSerializer`, like the
  development runner's `python bot.py -t websocket`, with the `wsUrl` and
  `token` start endpoints return:
  - `WebSocketTransportOptions` sets the audio sample rate and channels.
  - The bot's audio is converted to the app's sample rate and channels with
    speexdsp, and dropped when the bot is interrupted.
  - The `websocket` component of the CMake package
    (`find_package(pipecat COMPONENTS websocket)` and `pipecat::websocket`),
    which also finds libdatachannel and speexdsp. Both are downloaded if
    they're not installed.
  - The examples can use it, with `--transport websocket`.

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

- In the Daily transport:
  - Daily Core C++ SDK 0.23.0 or newer. It's a shared library, so apps ship it
    with them (see the transport's README).
  - The Daily Core SDK is found through its CMake package: point
    `DailyCore_ROOT` or `CMAKE_PREFIX_PATH` to it, instead of setting
    `DAILY_CORE_PATH`.
  - Daily Core is told the version of the SDK the transport is built with,
    instead of a fixed one.
  - User audio is sent with a custom audio track, and bot audio is received
    from the bot's track, instead of through Daily Core's virtual microphone
    and speaker, which only one transport per app can use. User audio no
    longer goes through echo cancellation, so apps that play the bot through
    speakers need their platform's echo cancellation or headphones.

### Removed

- `rtvi::RTVIClient`, with its actions, service configuration and helpers
  (`send_action()`, `RTVIHelper`, `RTVILLMHelper`, ...). Pipecat bots no longer
  support them.

- `cmake/FindPipecat.cmake` and the `PIPECAT_SDK_PATH` environment variable.

- Deprecated RTVI messages: `bot-transcription` and `tts-text` (use
  `bot-output`), and `llm-function-call` (use `llm-function-call-in-progress`).

- From the Daily transport:
  - `rtvi::DailyVoiceClient` and `include/daily_rtvi.h`. Use a
    `pipecat::PipecatClient` with a `DailyTransport` instead.
  - Daily Bots support. Start your bots with Pipecat Cloud or your own start
    endpoint.
  - `cmake/FindDailyPipecat.cmake` and the `DAILY_PIPECAT_SDK_PATH`
    environment variable.
  - The Daily Bots examples and their Node.js server.

### Fixed

- Messages with missing or unexpected fields no longer crash the app. They're
  reported to `on_error()` instead.

- Calling `disconnect()` from a callback no longer deadlocks with transports
  that deliver events on a single thread, like Daily.

- Slow function calls no longer block other events.

- libcurl handles are no longer leaked when starting the bot fails.

- In the Daily transport:
  - Joining a room fails with a `TransportStartError` if Daily returns an
    error, e.g. because of a wrong token, instead of looking connected.
  - Rooms that don't need a token can be joined without one.
  - A second transport no longer breaks the first one, and Daily Core is shut
    down after the last transport is destroyed.
