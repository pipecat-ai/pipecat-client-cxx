# Example

This is a simple example that outputs received bot messages to the terminal.

# Building

Before building the example we need to declare a few environment variables:

```bash
export DAILY_BOTS_PATH=/path/to/daily-bots-sdk
```

After that, it should be as simple as running:

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

# Cross-compiling (Linux aarch64)

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

# Usage

After building the example you should be able to run it:


```bash
./build/daily-bots-example
```

| Argument | Description         |
|----------|---------------------|
| -b       | Daily Bots base URL |
| -c       | Configuration file  |

There's a sample [configuration file](config.json).
