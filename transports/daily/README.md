# Daily Pipecat C++ client SDK

`pipecat-client-cxx-daily` is a C++ SDK to build native Pipecat client
applications with [Daily](https://www.daily.co/products/daily-bots/).

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

For a quickstart check the [Examples](#examples) section below.

# Dependencies

## Daily Core C++ SDK

Daily Pipecat C++ client SDK requires the [Daily Core C++
SDK](https://github.com/daily-co/daily-core-sdk) to be able to connect to
Daily's infrastructure. You can download it from the [available
releases](https://github.com/daily-co/daily-core-sdk/releases) for your
platform.

Then, define the following environment variable:

```
DAILY_CORE_PATH=/path/to/daily-core-sdk
```

## Pipecat C++ client SDK

It also requires the base [Pipecat C++ client
SDK](https://github.com/pipecat-ai/pipecat-client-cxx). Please, follow the
instructions on that project to build it.

Then, define the following environment variable:

```
PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
```

# Building

Before building the example we need to declare a few environment variables:

```bash
PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
DAILY_CORE_PATH=/path/to/daily-core-sdk
```

## Linux and macOS

```bash
cmake . -G Ninja -Bbuild
ninja -C build
```

## Windows

Initialize the command-line development environment.

```bash
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64
```

And then configure and build:

```bash
cmake . -Bbuild --preset vcpkg
cmake --build build --config Release
```

# Cross-compiling (Linux aarch64)

It is possible to build the example for the `aarch64` architecture in Linux with:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake
ninja -C build
```

# Security

To avoid sharing API keys in the client (including the Daily Bots API key) or if
you want to use your custom API keys for different services (e.g. OpenAI) you
will need to deploy a custom web server to be a proxy to the Daily Bots
API. This repo has a very simple server that you can use:

```bash
cd examples/server
npm install
node server.js
```

This will expose http://localhost:3000/start which is the URL you should use
instead.

# Examples

These are the list of available examples:

- [Daily Bots C++ client example](./examples/c++)
- [Daily Bots C++ client example with audio support (PortAudio)](./examples/c++-portaudio)
- An optional [Daily Bots Node.js server example](./examples/server)

## Quickstart (Linux and macOS)

The following are quickstart instructions for Linux and macOS. For Windows, go
to one of the examples above for instructions.

The first thing to do is build the Daily Pipecat C++ client library as described
above:

```bash
export PIPECAT_SDK_PATH=/PATH/TO/pipecat-client-cxx
export DAILY_CORE_PATH=/PATH/TO/daily-core-sdk-X.Y.Z-PLATFORM
cmake . -G Ninja -Bbuild
ninja -C build
```

Then, just build one of the examples:

```bash
cd examples/c++-portaudio
export DAILY_PIPECAT_SDK_PATH=/PATH/TO/pipecat-client-c++-daily
cmake . -G Ninja -Bbuild
ninja -C build
```

Before running the example make sure you have your Daily Bots API key setup:

```bash
export DAILY_BOTS_API_KEY=...
```

Finally, you can just try:

```bash
./build/example_audio -b https://api.daily.co/v1/bots/start -c config.json
```
