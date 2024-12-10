<h1><div align="center">
 <img alt="pipecat" width="300px" height="auto" src="https://raw.githubusercontent.com/pipecat-ai/pipecat-client-cxx/main/pipecat-cxx.png">
</div></h1>

[![Discord](https://img.shields.io/discord/1239284677165056021)](https://discord.gg/pipecat)

`pipecat-client-cxx` is a C++ SDK to build native [Pipecat](https://pipecat.ai) client applications.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

# Dependencies

## libcurl

We use [libcurl](https://curl.se/libcurl/) to make HTTP requests.

### Linux

```bash
sudo apt-get install libcurl4-openssl-dev
```

### macOS

On macOS `libcurl` is already included so there is nothing to install.

### Windows

On Windows we use [vcpkg](https://vcpkg.io/en/) to install dependencies. You
need to set it up following one of the
[tutorials](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started).

The `libcurl` dependency will be automatically downloaded when building.

# Building

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

## Cross-compiling (Linux aarch64)

It is possible to build the example for the `aarch64` architecture in Linux with:

```bash
cmake . -G Ninja -Bbuild -DCMAKE_TOOLCHAIN_FILE=aarch64-linux-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
ninja -C build
```
