<h1><div align="center">
 <img alt="pipecat" width="500px" height="auto" src="https://raw.githubusercontent.com/pipecat-ai/pipecat-client-cxx/main/pipecat-cxx.png">
</div></h1>

[![Docs](https://img.shields.io/badge/Documentation-blue)](https://docs.pipecat.ai) [![Discord](https://img.shields.io/discord/1239284677165056021)](https://discord.gg/pipecat)

# Pipecat C++ Client SDK

`pipecat-client-cxx` is a C++ SDK to build native [Pipecat](https://pipecat.ai) client applications.

It supports Linux (`x86_64` and `aarch64`), macOS (`aarch64`) and Windows
(`x86_64`).

## 🌐 Transports

`pipecat-client-cxx` needs a transport in order to connect to your Pipecat
bot. Currently available transports:

- [Daily Transport for Pipecat C++ Client SDK](https://github.com/pipecat-ai/pipecat-client-cxx-daily)

## 📦 Dependencies

- [libcurl](https://curl.se/libcurl/) to make HTTP requests.
- [nlohmann/json](https://github.com/nlohmann/json) 3.7 or newer. If it's not
  installed, CMake downloads it.

#### Linux

```bash
sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev
```

#### macOS

On macOS `libcurl` is already included, so you only need `nlohmann/json`:

```bash
brew install nlohmann-json
```

#### Windows

On Windows we use [vcpkg](https://vcpkg.io/en/) to install dependencies. You
need to set it up following one of the
[tutorials](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started).

The dependencies will be automatically downloaded when building.

## 🛠️ Building

You need a C++17 compiler (GCC 9 or newer, Clang, Apple Clang or MSVC) and
CMake 3.16 or newer.

### Linux and macOS

```bash
cmake . -G Ninja -Bbuild -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### Windows

Initialize the command-line development environment.

```bash
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64
```

And then configure and build:

```bash
cmake . -Bbuild --preset vcpkg
cmake --build build --config Release
```

### Cross-compiling (Linux aarch64)

It is possible to build the library for the `aarch64` architecture in Linux
with:

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
