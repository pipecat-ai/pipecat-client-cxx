# Examples

- [text](text): chat with a bot in the terminal. Type a message and the bot
  answers in text. No audio devices needed.

## Building

First build and install the Pipecat C++ client and this transport, following
the main [README](../README.md).

Then, from this directory, build the example:

```bash
cmake -S text -B build-text -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/pipecat;/path/to/pipecat_daily" \
  -DDailyCore_ROOT=/path/to/daily-core-sdk
ninja -C build-text
```

On Windows, the example gets its dependencies from
[vcpkg](https://vcpkg.io/en/):

```bash
cmake -S text -B build-text -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH="C:/path/to/pipecat;C:/path/to/pipecat_daily" -DDailyCore_ROOT=C:/path/to/daily-core-sdk
cmake --build build-text --config Release
```

## Running

The example starts a bot through its start endpoint, then connect to it. Pass
the endpoint's URL:

```bash
./build-text/text_chat http://localhost:7860/start
```

On Windows, it's in `build-text\Release`.

To run a bot on your machine, start any Pipecat bot that supports Daily with
the development runner, e.g. `python bot.py -t daily`. Its start endpoint is
`http://localhost:7860/start`, and it needs a `DAILY_API_KEY` to create rooms.

For a bot on [Pipecat Cloud](https://docs.pipecat.ai/pipecat-cloud), use your
agent's start endpoint and set your public API key:

```bash
export PIPECAT_API_KEY=pk_...
./build-text/text_chat https://api.pipecat.daily.co/v1/public/AGENT/start
```
