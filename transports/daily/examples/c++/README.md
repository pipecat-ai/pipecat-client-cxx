# Example

This is a simple example that outputs received bot messages to the terminal.

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
./build/example -b https://api.daily.co/v1/bots/start -c config.json
```

You can take a look at the sample [configuration file](config.json).
