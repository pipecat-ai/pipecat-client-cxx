<h1><div align="center">
 <img alt="pipecat" width="500px" height="auto" src="https://raw.githubusercontent.com/pipecat-ai/pipecat-client-cxx/main/pipecat-cxx.png">
</div></h1>

[![Docs](https://img.shields.io/badge/Documentation-blue)](https://docs.pipecat.ai) [![Discord](https://img.shields.io/discord/1239284677165056021)](https://discord.gg/pipecat)

# Pipecat C++ Client SDK

`pipecat-client-cxx` is a C++ SDK to build native [Pipecat](https://pipecat.ai)
client applications, such as robots, kiosks, smart devices or games, that talk
to a Pipecat bot.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`). It needs a C++17 compiler.

## 🌐 Transports

`pipecat-client-cxx` needs a transport to connect to your Pipecat bot. These
are the available transports (see [Building transports](#-building-transports)):

- [Daily](transports/daily): connects to the bot's Daily room, using WebRTC.
- [WebSocket](transports/websocket): connects to the bot's WebSocket.

You can also [write your own](#-writing-a-transport).

## 🚀 Usage

```cpp
#include <pipecat/pipecat.h>

#include <iostream>

class App : public pipecat::PipecatClientCallbacks {
   public:
    void on_bot_ready(const pipecat::rtvi::BotReadyData& data) override {
        std::cout << "Bot ready" << std::endl;
    }

    void on_user_transcript(const pipecat::rtvi::TranscriptData& data) override {
        if (data.final) {
            std::cout << "User: " << data.text << std::endl;
        }
    }

    void on_bot_output(const pipecat::rtvi::BotOutputData& data) override {
        std::cout << "Bot: " << data.text << std::endl;
    }
};

int main() {
    App app;

    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<MyTransport>();  // e.g. Daily
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

    // Handle an LLM function call. `respond` can also be called later, from
    // any thread, e.g. when a slow operation finishes.
    client.register_function_call_handler(
            "get_weather",
            [](const auto& params, auto respond) {
                respond({{"conditions", "sunny"}, {"temperature", 22}});
            }
    );

    // Start the bot (e.g. on Pipecat Cloud or your own server) and wait
    // until it's ready.
    pipecat::APIRequest request;
    request.endpoint = "https://example.com/start";
    client.start_bot_and_connect(request);

    client.send_text("Hello!");

    // Meanwhile, your audio threads send user audio and play bot audio (16-bit
    // PCM) with client.send_user_audio() and client.read_bot_audio().

    ...

    client.disconnect();
}
```

The client can also send app-defined messages to your bot and wait for its
answer, with a callback or a `std::future`:

```cpp
auto weather = client.send_client_request("get-weather", {{"city", "SF"}}).get();
```

## 💡 Examples

- [text](examples/text): chat with a bot in the terminal.
- [voice](examples/voice): talk with a bot using your microphone and speakers.

They use the Daily transport. See [examples/README.md](examples/README.md) to
build them and run them with a bot on your machine or on Pipecat Cloud.

## 🧵 Threading

- `start_bot()`, `connect()` and `disconnect()` block the calling thread.
  Everything else returns right away. All methods are thread-safe.
- Callbacks, `send_client_request()` callbacks and function call handlers run
  on the client's own event thread, one at a time and in order. You can call
  any client method from them, including `disconnect()` or waiting on a
  `send_client_request()` future.
- Events wait while a callback runs, so keep callbacks short. Function call
  handlers can respond later instead of blocking.
- Callbacks must not throw, and must not destroy the client.
- Call `send_user_audio()` and `read_bot_audio()` from your own audio threads.

## 📚 Documentation

- Guides: [docs.pipecat.ai](https://docs.pipecat.ai)
- API reference: [docs-cxx.pipecat.ai](https://docs-cxx.pipecat.ai)
- Changes and migration from 0.x: [CHANGELOG.md](CHANGELOG.md)

## 🛠️ Building

You need a C++17 compiler (GCC 9 or newer, Clang, Apple Clang or MSVC) and
CMake 3.16 or newer. The client uses [libcurl](https://curl.se/libcurl/) to
make HTTP requests, and [nlohmann/json](https://github.com/nlohmann/json) 3.7
or newer, which CMake downloads if it's not installed.

### Linux

```bash
sudo apt-get install cmake ninja-build libcurl4-openssl-dev nlohmann-json3-dev
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### macOS

macOS already includes libcurl:

```bash
brew install cmake ninja nlohmann-json
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### Windows

The dependencies come from [vcpkg](https://vcpkg.io/en/), which downloads
them when you configure. Set it up following one of its
[tutorials](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started),
so that `VCPKG_ROOT` points to it. Then:

```bash
cmake --preset vcpkg
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

You need an `aarch64` cross compiler (`g++-aarch64-linux-gnu` on Debian and
Ubuntu) and the `aarch64` version of libcurl. Then:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

## 📥 Installing

```bash
cmake --install build --prefix /path/to/pipecat
```

Then, in your CMake project, point `CMAKE_PREFIX_PATH` to that directory and
use:

```cmake
find_package(pipecat 1.0 REQUIRED)
target_link_libraries(my_app PRIVATE pipecat::pipecat)
```

You can also include this repository with `add_subdirectory()` or
`FetchContent` and link to the same `pipecat::pipecat` target.

A `pipecat.pc` file is also installed for Meson and other build systems that
use pkg-config. Add `/path/to/pipecat/lib/pkgconfig` to `PKG_CONFIG_PATH` and
use:

```meson
pipecat_dep = dependency('pipecat', version: '>= 1.0')
```

Meson projects need `cpp_std=c++17` or newer.

## 🧩 Building transports

Transports are off by default, since each needs its own dependencies. Turn on
the ones you want when configuring, and they're built, tested and installed
with the client:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release \
  -DPIPECAT_BUILD_DAILY=ON \
  -DDailyCore_ROOT=/path/to/daily-core-sdk \
  -DPIPECAT_BUILD_WEBSOCKET=ON
```

| Transport | Option | Component | Needs |
| --- | --- | --- | --- |
| [Daily](transports/daily) | `PIPECAT_BUILD_DAILY` | `daily` | The [Daily Core C++ SDK](https://github.com/daily-co/daily-core-sdk) 0.23.0 or newer, with `DailyCore_ROOT` pointing to it |
| [WebSocket](transports/websocket) | `PIPECAT_BUILD_WEBSOCKET` | `websocket` | [libdatachannel](https://github.com/paullouisageneau/libdatachannel) 0.24 or newer, which CMake downloads if it's not installed (with OpenSSL), and [speexdsp](https://github.com/xiph/speexdsp) |

Then use them as components of the package, which also finds their
dependencies. Each component's target is `pipecat::<component>`:

```cmake
find_package(pipecat 1.0 REQUIRED COMPONENTS daily)
target_link_libraries(my_app PRIVATE pipecat::daily)
```

With `add_subdirectory()` or `FetchContent`, set the options before adding
this repository.

## 🔌 Writing a transport

Implement `pipecat::Transport` (see `include/pipecat/transport.h`):

- Report messages from the bot, participants joining and leaving, errors and
  unexpected disconnections to the `TransportObserver` given to
  `initialize()`. Observer calls never block, so you can make them from any
  thread, including event threads that must never block.
- `connect()` and `disconnect()` can block until they're done. The client never
  calls them from inside an observer call.
- `send_ready_message()` receives the `client-ready` message. Send it once the
  bot can receive it, which may be after `connect()` returns.

## 🧪 Testing

Unit tests use [GoogleTest](https://github.com/google/googletest), which is
downloaded if it's not installed. They are built by default, except when
cross-compiling, and you can turn them off with `-DPIPECAT_BUILD_TESTS=OFF`.

```bash
cd build && ctest --output-on-failure
```

## 📖 Building the API reference

The API reference is generated with [Doxygen](https://www.doxygen.nl):

```bash
cmake . -G Ninja -Bbuild -DPIPECAT_BUILD_DOCS=ON
ninja -C build docs
```

It's written to `build/docs/html`.
