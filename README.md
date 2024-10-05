# Daily Bots SDK

`daily-bots-sdk` is an SDK to build native applications for the [Daily
Bots](https://www.daily.co/products/daily-bots/) platform with a simple static
library and a C header file.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

For a quickstart check the [Examples](#examples) section below.

# Dependencies

Daily Bots SDK requires the [Daily Core
SDK](https://github.com/daily-co/daily-core-sdk) to be able connect to Daily's
cloud infrastructure. Simply download the desired Daily Core SDK release for
your platform from the available
[releases](https://github.com/daily-co/daily-core-sdk/releases).

Then, define the following environment variable:

```
DAILY_CORE_PATH=/path/to/daily-core-sdk
```

# Building

Generate build files:

```bash
cmake . -G Ninja -Bbuild
```

You can also specify if you want `Debug` (default) or `Release`:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release
```

then, just compile:

```bash
ninja -C build
```

## Cross-compiling (Linux aarch64)

It is possible to build the example for the `aarch64` architecture in Linux with:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake
```

or in release mode:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

And finally, as before:

```bash
ninja -C build
```

# Examples

These are the list of available examples:

- [Daily Bots C++ client example](./examples/c++)
- [Daily Bots C++ client example with audio support (PortAudio)](./examples/c++-portaudio)
- An optional [Daily Bots Node.js server example](./examples/server)

The first thing to do is build the Daily Bots library as described above:

```bash
export DAILY_CORE_PATH=/PATH/TO/daily-core-sdk-X.Y.Z-PLATFORM
cmake . -G Ninja -Bbuild
ninja -C build
```

Then, just build one of the examples:

```bash
cd examples/c++-portaudio
export DAILY_BOTS_PATH=/PATH/TO/daily-bots-sdk
export DAILY_CORE_PATH=/PATH/TO/daily-core-sdk-X.Y.Z-PLATFORM
cmake . -G Ninja -Bbuild
ninja -C build
```

Before running the example make sure you have your Daily Bots API key setup:

```bash
export DAILY_BOTS_API_KEY=...
```

Finally, you can just try:

```bash
./build/daily-bots-audio -b http://localhost:3000 -c config.json
```

If you want to use your custom API keys for different services (e.g. OpenAI) you
will want to deploy custom web server to be a proxy to the Daily Bots API. This
repo has a very simple server that you can use:

```bash
cd examples/server
npm install
node server.js
```

This will expose http://localhost:3000 which is the base URL that we will use
below.
