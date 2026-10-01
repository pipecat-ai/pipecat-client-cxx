# Examples

- [text](text): chat with a bot in the terminal. Type a message and the bot
  answers in text. No audio devices needed.
- [voice](voice): talk with a bot using your microphone and speakers, with
  [PortAudio](https://www.portaudio.com). It also answers the bot's
  `get_current_time` function calls, if the bot asks the client to run them.
  Use headphones: the example doesn't cancel echo, so with speakers the bot
  hears itself.

## Building

The examples use the Daily transport. First build and install the Pipecat C++
client with it, see
[Building transports](../README.md#-building-transports).

The voice example also needs PortAudio:

```bash
# Linux
sudo apt-get install portaudio19-dev pkg-config

# macOS
brew install portaudio pkgconf
```

Then, from this directory, build an example in its own build directory. For
the text example:

```bash
cmake -S text -B build-text -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/pipecat \
  -DDailyCore_ROOT=/path/to/daily-core-sdk
ninja -C build-text
```

For the voice example, replace `text` with `voice`.

On Windows, the examples get their dependencies from
[vcpkg](https://vcpkg.io/en/), and the build copies `daily_core.dll` next to
them:

```bash
cmake -S text -B build-text -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/path/to/pipecat -DDailyCore_ROOT=C:/path/to/daily-core-sdk
cmake --build build-text --config Release
```

## Running

The examples start a bot through its start endpoint, then connect to it. Pass
the endpoint's URL:

```bash
./build-text/text_chat http://localhost:7860/start
./build-voice/voice_chat http://localhost:7860/start
```

On Windows, they're in `build-text\Release` and `build-voice\Release`.

To run a bot on your machine, start any Pipecat bot that supports Daily with
the development runner, e.g. `python bot.py -t daily`. Its start endpoint is
`http://localhost:7860/start`, and it needs a `DAILY_API_KEY` to create rooms.

For a bot on [Pipecat Cloud](https://docs.pipecat.ai/pipecat-cloud), use your
agent's start endpoint and set your public API key:

```bash
export PIPECAT_API_KEY=pk_...
./build-text/text_chat https://api.pipecat.daily.co/v1/public/AGENT/start
```
