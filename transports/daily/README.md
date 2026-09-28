<h1><div align="center">
 <img alt="pipecat" width="500px" height="auto" src="https://raw.githubusercontent.com/pipecat-ai/pipecat-client-cxx-daily/main/pipecat-cxx.png">
</div></h1>

[![Docs](https://img.shields.io/badge/Documentation-blue)](https://docs.pipecat.ai) [![Discord](https://img.shields.io/discord/1239284677165056021)](https://discord.gg/pipecat)

# Daily Transport for Pipecat C++ Client SDK

`pipecat-client-cxx-daily` is a transport for the
[Pipecat C++ Client SDK](https://github.com/pipecat-ai/pipecat-client-cxx)
that connects to [Pipecat](https://pipecat.ai) bots over
[Daily](https://www.daily.co), using WebRTC.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`). It needs a C++17 compiler.

## 🚀 Usage

Give a `pipecat::DailyTransport` to the client when you create it:

```cpp
#include <pipecat/daily/transport.h>
#include <pipecat/pipecat.h>

int main() {
    App app;  // Your pipecat::PipecatClientCallbacks.

    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::DailyTransport>();
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

    // Start a bot on Pipecat Cloud in a new Daily room, and connect to it.
    pipecat::APIRequest request;
    request.endpoint = "https://api.pipecat.daily.co/v1/public/AGENT/start";
    request.headers = {{"Authorization", "Bearer " + api_key}};
    request.request_data = {{"createDailyRoom", true}};
    client.start_bot_and_connect(request);

    ...

    client.disconnect();
}
```

Then use the client as usual, see the
[Pipecat C++ Client SDK](https://github.com/pipecat-ai/pipecat-client-cxx).

- The transport joins the bot's Daily room. Start endpoints like Pipecat Cloud
  or Pipecat's development runner (`python bot.py -t daily`) create one when
  you ask with `createDailyRoom`. If you already have a room, connect to it
  directly with `client.connect({{"url", room_url}, {"token", token}})`.
- Audio is 16-bit PCM, 16 kHz mono by default. Change it with
  `pipecat::DailyTransportOptions`. The user's audio is sent without echo
  cancellation, so if you play the bot through speakers, use your platform's
  echo cancellation or headphones.
- Each client needs its own `DailyTransport`, and several clients can run at
  the same time, e.g. to talk to several bots.

## 💡 Examples

- [text](examples/text): chat with a bot in the terminal.
- [voice](examples/voice): talk with a bot using your microphone and speakers.

See [examples/README.md](examples/README.md) to build them and run them with
a bot on your machine or on Pipecat Cloud.

## 📚 Documentation

- Guides: [docs.pipecat.ai](https://docs.pipecat.ai)
- Pipecat C++ Client SDK API reference:
  [docs-cxx.pipecat.ai](https://docs-cxx.pipecat.ai)
- Changes and migration from 0.x: [CHANGELOG.md](CHANGELOG.md)

## 🛠️ Building

You need:

- A C++17 compiler (GCC 9 or newer, Clang, Apple Clang or MSVC) and CMake
  3.16 or newer.
- The [Pipecat C++ Client SDK](https://github.com/pipecat-ai/pipecat-client-cxx)
  1.0. Build and install it following its README.
- The [Daily Core C++ SDK](https://github.com/daily-co/daily-core-sdk) 0.23.0
  or newer. Download it for your platform from its
  [releases](https://github.com/daily-co/daily-core-sdk/releases) and unpack
  it.

### Linux and macOS

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/pipecat \
  -DDailyCore_ROOT=/path/to/daily-core-sdk
ninja -C build
```

### Windows

Like the Pipecat client, this gets libcurl and nlohmann/json from
[vcpkg](https://vcpkg.io/en/), so `VCPKG_ROOT` needs to point to it. Then:

```bash
cmake --preset vcpkg -DCMAKE_PREFIX_PATH=C:/path/to/pipecat -DDailyCore_ROOT=C:/path/to/daily-core-sdk
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

Build the Pipecat client for `aarch64` too, and use the `linux-arm64` Daily
Core SDK:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/pipecat-aarch64 \
  -DDailyCore_ROOT=/path/to/daily-core-sdk-linux-arm64
ninja -C build
```

## 📥 Installing

```bash
cmake --install build --prefix /path/to/pipecat_daily
```

Then, in your CMake project, point `CMAKE_PREFIX_PATH` to both the Pipecat
client and this transport, `DailyCore_ROOT` to the Daily Core SDK, and use:

```cmake
find_package(pipecat_daily 1.0 REQUIRED)
target_link_libraries(my_app PRIVATE pipecat::daily)
```

`pipecat::daily` also links the Pipecat client and the Daily Core SDK.

Daily Core is a shared library, so ship it with your app. On Windows, put
`daily_core.dll` next to your `.exe`, like the examples do. See
[Shipping the library](https://github.com/daily-co/daily-core-sdk#shipping-the-library)
in the Daily Core SDK's README.

You can also include this repository with `add_subdirectory()` or
`FetchContent`, after the Pipecat client, and link to the same
`pipecat::daily` target.

## 🧪 Testing

Unit tests use [GoogleTest](https://github.com/google/googletest), which is
downloaded if it's not installed. They are built by default, except when
cross-compiling, and you can turn them off with `-DPIPECAT_DAILY_BUILD_TESTS=OFF`.

```bash
cd build && ctest --output-on-failure
```

## 📖 Building the API reference

The API reference is generated with [Doxygen](https://www.doxygen.nl). Add
`-DPIPECAT_DAILY_BUILD_DOCS=ON` when configuring, and then:

```bash
ninja -C build docs
```

It's written to `build/docs/html`.
