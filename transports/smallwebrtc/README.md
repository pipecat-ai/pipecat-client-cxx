# SmallWebRTC transport

`pipecat::SmallWebRTCTransport` is a transport for the
[Pipecat C++ Client SDK](../../README.md) that connects to
[Pipecat](https://pipecat.ai) bots over WebRTC, peer to peer, with Pipecat's
SmallWebRTC transport.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

## 🚀 Usage

Give a `pipecat::SmallWebRTCTransport` to the client when you create it:

```cpp
#include <pipecat/pipecat.h>
#include <pipecat/smallwebrtc/transport.h>

int main() {
    App app;  // Your pipecat::PipecatClientCallbacks.

    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::SmallWebRTCTransport>();
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

    // Start a bot with Pipecat's development runner, and connect to it.
    pipecat::APIRequest request;
    request.endpoint = "http://localhost:7860/start";
    request.request_data = {{"transport", "webrtc"}};
    client.start_bot_and_connect(request);

    ...

    client.disconnect();
}
```

Then use the client as usual, see the
[Pipecat C++ Client SDK](../../README.md).

- The transport sends a WebRTC offer to the bot's server. After
  `start_bot()`, it finds out where on its own: start endpoints like Pipecat's
  development runner (`python bot.py -t webrtc`) and Pipecat Cloud answer
  with a session ID when you ask for the `webrtc` transport, and the offer
  goes to the same server, with the same headers. If your app already knows
  the offer endpoint, connect directly with
  `client.connect({{"webrtcRequestParams", {{"endpoint", offer_url}}}})`.
- STUN and TURN servers come from the start endpoint's answer, in
  `iceConfig`. Pipecat's development runner returns the ones it was started
  with (`--ice-servers`), or a public STUN server if you add
  `{"enableDefaultIceServers", true}` to the request. Bots on other networks
  need them to be reached.
- Audio is 16-bit PCM, 16 kHz mono by default. Change it with
  `pipecat::SmallWebRTCTransportOptions`, at any sample rate. It's sent to the
  bot with Opus, in mono. The bot's audio plays at the pace the bot sent it,
  80 ms behind, so the network's ups and downs don't reach your app, and lost
  audio is concealed. The user's audio is sent without echo
  cancellation, so if you play the bot through speakers, use your platform's
  echo cancellation or headphones.
- Each client needs its own `SmallWebRTCTransport`, and several clients can
  run at the same time, e.g. to talk to several bots.

## 💡 Examples

The [examples](../../examples) can use the SmallWebRTC transport, with
`--transport smallwebrtc`:

- [text](../../examples/text): chat with a bot in the terminal.
- [voice](../../examples/voice): talk with a bot using your microphone and
  speakers.

## 📚 Documentation

- Guides: [docs.pipecat.ai](https://docs.pipecat.ai)
- API reference: [docs-cxx.pipecat.ai](https://docs-cxx.pipecat.ai), which
  includes the SmallWebRTC transport
- Changes: [CHANGELOG.md](../../CHANGELOG.md)

## 🛠️ Building

The transport is built with the Pipecat client, from the root of this
repository, when you add `-DPIPECAT_BUILD_SMALLWEBRTC=ON`. Besides what the
client needs (see its [README](../../README.md)), you need:

- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) 0.24 or
  newer, built with media support, for WebRTC. CMake downloads and builds it
  if it's not installed, which needs OpenSSL. A downloaded one is patched so
  the bot notices right away when the client disconnects. With an installed
  one, it notices about 30 seconds later.
- [libopus](https://opus-codec.org), for the audio. CMake downloads and builds
  it if it's not installed.
- [speexdsp](https://github.com/xiph/speexdsp), to convert the audio between
  sample rates. If it's not installed, CMake downloads it and builds what the
  transport needs into it.

### Linux

```bash
sudo apt-get install libopus-dev libspeexdsp-dev libssl-dev
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release -DPIPECAT_BUILD_SMALLWEBRTC=ON
ninja -C build
```

### macOS

Homebrew doesn't have libdatachannel, so CMake downloads it. It needs
Homebrew's OpenSSL, which isn't where CMake looks, so point
`OPENSSL_ROOT_DIR` to it:

```bash
brew install openssl@3 opus speexdsp
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release -DPIPECAT_BUILD_SMALLWEBRTC=ON
ninja -C build
```

### Windows

vcpkg installs libdatachannel, libopus and speexdsp with the `smallwebrtc`
feature:

```bash
cmake --preset vcpkg -DPIPECAT_BUILD_SMALLWEBRTC=ON -DVCPKG_MANIFEST_FEATURES=smallwebrtc
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

Besides what the client needs, you need the `aarch64` versions of OpenSSL,
and of libopus and speexdsp if you don't want CMake to download them. Then add
`-DPIPECAT_BUILD_SMALLWEBRTC=ON` to the client's
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
find_package(pipecat 1.0 REQUIRED COMPONENTS smallwebrtc)
target_link_libraries(my_app PRIVATE pipecat::smallwebrtc)
```

`pipecat::smallwebrtc` also links the Pipecat client, libdatachannel, libopus,
and speexdsp if it was installed. If CMake downloaded libdatachannel or
libopus, they're installed with the transport, as static libraries, and your
app also needs OpenSSL (on macOS, with `OPENSSL_ROOT_DIR` pointing to it).

You can also include this repository with `add_subdirectory()` or
`FetchContent`: set `PIPECAT_BUILD_SMALLWEBRTC` to `ON` before, and link to the
same `pipecat::smallwebrtc` target.
