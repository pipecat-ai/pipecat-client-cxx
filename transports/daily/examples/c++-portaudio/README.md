# Example

This example uses [PortAudio](https://www.portaudio.com/) to play audio from the
bot and record audio from the user.

# Dependencies

## PortAudio

### Linux

```bash
sudo apt-get install libportaudio2 portaudio19-dev
```

### macOS

```bash
brew install portaudio
```

### Windows

On Windows we use [vcpkg](https://vcpkg.io/en/) to install dependencies. You
need to set it up following one of the
[tutorials](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started).

The `portaudio` dependency will be automatically when building.

# Building

Before building the example we need to declare a few environment variables:

```bash
PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
DAILY_PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx-daily
DAILY_CORE_PATH=/path/to/daily-core-sdk
```

## Linux and macOS

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release
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
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

# Usage

After building the example you should be able to run it, just make sure you have
your Daily Bots API key setup:

```bash
export DAILY_BOTS_API_KEY=...
```

Here's how you would try it:

```bash
./build/example_audio -b https://api.daily.co/v1/bots/start -c config_voice_2024_10.json
```

You can take a look at the sample [configuration
file](config_voice_2024_10.json). This file uses the `voice_2024_10` Daily Bots
profile.

You can also use another profile such as `openai_realtime_beta_2024_10`. A
[configuration file](config_openai_realtime_beta_2024_10.json) is also
provided. In this case, Daily Bots requires that you provide your own OpenAI
Realtime API key which you can do directly in the configuration file for testing
purposes, however it's recommended that you use a proxy server that makes the
Daily Bots request for you as explained [here](../../README.md#security).

After adding your OpenAI Realtime API key you can run it with:

```bash
./build/example_audio -b https://api.daily.co/v1/bots/start -c config_openai_realtime_beta_2024_10.json
```
