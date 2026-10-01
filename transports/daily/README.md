# Daily transport

`pipecat::DailyTransport` is a transport for the
[Pipecat C++ Client SDK](../../README.md) that connects to
[Pipecat](https://pipecat.ai) bots over [Daily](https://www.daily.co), using
WebRTC.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

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
[Pipecat C++ Client SDK](../../README.md).

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

The [examples](../../examples) use the Daily transport:

- [text](../../examples/text): chat with a bot in the terminal.
- [voice](../../examples/voice): talk with a bot using your microphone and
  speakers.

## 📚 Documentation

- Guides: [docs.pipecat.ai](https://docs.pipecat.ai)
- API reference: [docs-cxx.pipecat.ai](https://docs-cxx.pipecat.ai), which
  includes the Daily transport
- Changes and migration from 0.x: [CHANGELOG.md](../../CHANGELOG.md)

## 🛠️ Building

The transport is built with the Pipecat client, from the root of this
repository, when you add `-DPIPECAT_BUILD_DAILY=ON`. Besides what the client
needs (see its [README](../../README.md)), you need the
[Daily Core C++ SDK](https://github.com/daily-co/daily-core-sdk) 0.23.0 or
newer. Download it for your platform from its
[releases](https://github.com/daily-co/daily-core-sdk/releases) and unpack it.

### Linux and macOS

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release \
  -DPIPECAT_BUILD_DAILY=ON \
  -DDailyCore_ROOT=/path/to/daily-core-sdk
ninja -C build
```

### Windows

```bash
cmake --preset vcpkg -DPIPECAT_BUILD_DAILY=ON -DDailyCore_ROOT=C:/path/to/daily-core-sdk
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

Use the `linux-arm64` Daily Core SDK:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
  -DPIPECAT_BUILD_DAILY=ON \
  -DDailyCore_ROOT=/path/to/daily-core-sdk-linux-arm64
ninja -C build
```

Its tests are built and run with the client's.

## 📥 Installing

This installs the client and the transport:

```bash
cmake --install build --prefix /path/to/pipecat
```

Then, in your CMake project, point `CMAKE_PREFIX_PATH` to that directory,
`DailyCore_ROOT` to the Daily Core SDK, and use:

```cmake
find_package(pipecat 1.0 REQUIRED COMPONENTS daily)
target_link_libraries(my_app PRIVATE pipecat::daily)
```

`pipecat::daily` also links the Pipecat client and the Daily Core SDK.

Daily Core is a shared library, so ship it with your app. On Windows, put
`daily_core.dll` next to your `.exe`, like the examples do. See
[Shipping the library](https://github.com/daily-co/daily-core-sdk#shipping-the-library)
in the Daily Core SDK's README.

You can also include this repository with `add_subdirectory()` or
`FetchContent`: set `PIPECAT_BUILD_DAILY` to `ON` before, and link to the same
`pipecat::daily` target.
