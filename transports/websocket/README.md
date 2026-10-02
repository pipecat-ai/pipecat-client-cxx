# WebSocket transport

`pipecat::WebSocketTransport` is a transport for the
[Pipecat C++ Client SDK](../../README.md) that connects to
[Pipecat](https://pipecat.ai) bots over a WebSocket.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

## 🚀 Usage

Give a `pipecat::WebSocketTransport` to the client when you create it:

```cpp
#include <pipecat/pipecat.h>
#include <pipecat/websocket/transport.h>

int main() {
    App app;  // Your pipecat::PipecatClientCallbacks.

    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::WebSocketTransport>();
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

    // Start a bot with Pipecat's development runner, and connect to it.
    pipecat::APIRequest request;
    request.endpoint = "http://localhost:7860/start";
    request.request_data = {{"transport", "websocket"}};
    client.start_bot_and_connect(request);

    ...

    client.disconnect();
}
```

Then use the client as usual, see the
[Pipecat C++ Client SDK](../../README.md).

- The transport connects to the bot's WebSocket. Start endpoints like
  Pipecat's development runner (`python bot.py -t websocket`) return its URL,
  and a token if it needs one, when you ask for the `websocket` transport. If
  you already have them, connect directly with
  `client.connect({{"wsUrl", ws_url}, {"token", token}})`.
- The bot needs a WebSocket transport with Pipecat's `ProtobufFrameSerializer`,
  like the [websocket](https://github.com/pipecat-ai/pipecat-examples/tree/main/websocket)
  example's bot.
- Audio is 16-bit PCM, 16 kHz mono by default. Change it with
  `pipecat::WebSocketTransportOptions`. The bot's audio is converted to it from
  whatever the bot sends. The user's audio is sent without echo cancellation,
  so if you play the bot through speakers, use your platform's echo
  cancellation or headphones.
- Each client needs its own `WebSocketTransport`, and several clients can run
  at the same time, e.g. to talk to several bots.

## 💡 Examples

The [examples](../../examples) can use the WebSocket transport, with
`--transport websocket`:

- [text](../../examples/text): chat with a bot in the terminal.
- [voice](../../examples/voice): talk with a bot using your microphone and
  speakers.

## 📚 Documentation

- Guides: [docs.pipecat.ai](https://docs.pipecat.ai)
- API reference: [docs-cxx.pipecat.ai](https://docs-cxx.pipecat.ai), which
  includes the WebSocket transport
- Changes: [CHANGELOG.md](../../CHANGELOG.md)

## 🛠️ Building

The transport is built with the Pipecat client, from the root of this
repository, when you add `-DPIPECAT_BUILD_WEBSOCKET=ON`. Besides what the
client needs (see its [README](../../README.md)), you need:

- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) 0.24 or
  newer, for its WebSocket client. CMake downloads and builds it if it's not
  installed, which needs OpenSSL.
- [speexdsp](https://github.com/xiph/speexdsp), to convert the bot's audio.
  If it's not installed, CMake downloads it and builds what the transport
  needs into it.

### Linux

```bash
sudo apt-get install libssl-dev
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release -DPIPECAT_BUILD_WEBSOCKET=ON
ninja -C build
```

### macOS

Homebrew doesn't have libdatachannel, so CMake downloads it. It needs
Homebrew's OpenSSL, which isn't where CMake looks, so point
`OPENSSL_ROOT_DIR` to it:

```bash
brew install openssl@3
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release -DPIPECAT_BUILD_WEBSOCKET=ON
ninja -C build
```

### Windows

vcpkg installs libdatachannel and speexdsp with the `websocket` feature:

```bash
cmake --preset vcpkg -DPIPECAT_BUILD_WEBSOCKET=ON -DVCPKG_MANIFEST_FEATURES=websocket
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

Besides what the client needs, you need the `aarch64` version of OpenSSL.
Then add `-DPIPECAT_BUILD_WEBSOCKET=ON` to the client's
[cross-compiling](../../README.md#cross-compiling-linux-aarch64) command.

Its tests are built and run with the client's.

## 📥 Installing

This installs the client and the transport:

```bash
cmake --install build --prefix /path/to/pipecat
```

Then, in your CMake project, point `CMAKE_PREFIX_PATH` to that directory and
use:

```cmake
find_package(pipecat 1.0 REQUIRED COMPONENTS websocket)
target_link_libraries(my_app PRIVATE pipecat::websocket)
```

`pipecat::websocket` also links the Pipecat client, libdatachannel, and
speexdsp if it was installed. If CMake downloaded libdatachannel, it's
installed with the transport, as a static library, and your app also needs
OpenSSL (on macOS, with `OPENSSL_ROOT_DIR` pointing to it).

You can also include this repository with `add_subdirectory()` or
`FetchContent`: set `PIPECAT_BUILD_WEBSOCKET` to `ON` before, and link to the
same `pipecat::websocket` target.
