# Overview

The Pipecat C++ Client SDK connects native apps, like robots, kiosks, devices
or games, to [Pipecat](https://pipecat.ai) bots. It speaks RTVI 2.1, the
protocol Pipecat clients and bots use to talk to each other.

This is the API reference. To install the SDK and get started, see the
[README](https://github.com/pipecat-ai/pipecat-client-cxx) and the guides at
[docs.pipecat.ai](https://docs.pipecat.ai).

## How it fits together

```
+----------+   methods   +---------------+          +-----------+           +-------------+
|          | ----------> |               | -------> |           | --------> |             |
| Your app |             | PipecatClient |          | Transport |  network  | Pipecat bot |
|          | <---------- |               | <------- |           | <-------- |             |
+----------+  callbacks  +---------------+  events  +-----------+           +-------------+
```

- Your app uses a [PipecatClient](@ref pipecat::PipecatClient) to start and
  connect to a bot, send it messages and audio, and disconnect.
- The client tells your app what happens, like the bot being ready or what
  the user said, through
  [PipecatClientCallbacks](@ref pipecat::PipecatClientCallbacks).
- A [Transport](@ref pipecat::Transport) carries messages and audio between
  the client and the bot, e.g. over WebRTC. You choose one when you create the
  client. To write your own, implement [Transport](@ref pipecat::Transport)
  and send its events to the
  [TransportObserver](@ref pipecat::TransportObserver) it's given.

To create a client, give it a transport and your callbacks:

```cpp
class App : public pipecat::PipecatClientCallbacks {
   public:
    void on_bot_ready(const pipecat::rtvi::BotReadyData& data) override {
        std::cout << "The bot is ready" << std::endl;
    }
};

App app;

pipecat::PipecatClientOptions options;
options.transport = std::make_unique<MyTransport>();  // e.g. Daily
options.callbacks = &app;

pipecat::PipecatClient client(std::move(options));
```

## Threads

The client runs its own threads, so you only need yours for audio and for
slow work.

**Your calls.**
[start_bot()](@ref pipecat::PipecatClient::start_bot),
[connect()](@ref pipecat::PipecatClient::connect) and
[disconnect()](@ref pipecat::PipecatClient::disconnect) wait until they're
done, which can take a few seconds. Call them from a thread that can wait,
not from one that must stay responsive, like a UI thread. Everything else
returns right away. You can call any method from any thread.

**Callbacks** run on a thread the client creates, not on yours. They run one
at a time, in the order things happened. This means:

- Data you share between callbacks and your other threads needs a lock, e.g.
  a `std::mutex`.
- If your UI or engine can only be used from its main thread, pass events to
  that thread instead of using it in the callback.
- Other events wait while a callback runs, so keep callbacks short and do
  slow work somewhere else.
- Callbacks can call any client method, including
  [disconnect()](@ref pipecat::PipecatClient::disconnect).
- Callbacks must not throw, and must not destroy the client.

Request callbacks and function call handlers run on the same thread as the
other callbacks. A function call handler can respond later, from any thread,
so slow work doesn't hold up other events.

**Audio** is up to you: send and read it from your own audio threads, like
the ones your audio library gives you.

For example, to show what the bot says in a UI that must be used from its
main thread, hand the text over to it:

```cpp
class App : public pipecat::PipecatClientCallbacks {
   public:
    // Runs on the client's thread.
    void on_bot_output(const pipecat::rtvi::BotOutputData& data) override {
        std::lock_guard<std::mutex> lock(_mutex);
        _bot_text.push_back(data.text);
    }

    // Called by the main thread, e.g. once per frame.
    std::vector<std::string> take_bot_text() {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::exchange(_bot_text, {});
    }

   private:
    std::mutex _mutex;
    std::vector<std::string> _bot_text;
};
```

## Main types

| Type | What it's for |
| --- | --- |
| [PipecatClient](@ref pipecat::PipecatClient) | Connects to a bot, and sends it messages and audio. |
| [PipecatClientOptions](@ref pipecat::PipecatClientOptions) | Options to create a client, including its transport. |
| [PipecatClientCallbacks](@ref pipecat::PipecatClientCallbacks) | Receives events from the client. |
| [APIRequest](@ref pipecat::APIRequest) | A request to start a bot. |
| [TransportState](@ref pipecat::TransportState) | The state of the connection. |
| [Transport](@ref pipecat::Transport) and [TransportObserver](@ref pipecat::TransportObserver) | The interface to write transports. |
| [rtvi](@ref pipecat::rtvi) | RTVI messages and their data, e.g. [BotOutputData](@ref pipecat::rtvi::BotOutputData). |
| [PipecatError](@ref pipecat::PipecatError) | Base class of all the errors the client throws. |

## Transports

The SDK comes with these transports. Each is a separate library, built only
when you ask for it:

- [Daily](@ref daily): connects to bots in a Daily room, using WebRTC.

## Connecting

Connecting to a bot takes two steps:

1. **Start the bot.** [start_bot()](@ref pipecat::PipecatClient::start_bot)
   sends a request to a start endpoint, like Pipecat Cloud or your own
   server. The endpoint starts a bot and answers with what the transport needs
   to reach it, like a room URL and a token.
2. **Connect to it.** [connect()](@ref pipecat::PipecatClient::connect) gives
   that answer to the transport, which connects to the bot. It returns once
   the bot is ready to talk.

[start_bot_and_connect()](@ref pipecat::PipecatClient::start_bot_and_connect)
does both steps:

```cpp
pipecat::APIRequest request;
request.endpoint = "https://example.com/start";
request.headers = {{"Authorization", "Bearer " + api_key}};

try {
    client.start_bot_and_connect(request);
} catch (const pipecat::PipecatError& e) {
    std::cerr << "Unable to connect: " << e.what() << std::endl;
}
```

If your app already knows how to reach the bot, e.g. because your backend
started it, skip the first step and call
[connect()](@ref pipecat::PipecatClient::connect) directly. What it needs
depends on the transport:

```cpp
client.connect({{"room_url", room_url}, {"token", token}});
```

When you're done, [disconnect()](@ref pipecat::PipecatClient::disconnect) ends
the session:

```cpp
client.disconnect();
```

The state goes from `Disconnected` through `Authenticating`, `Connecting` and
`Connected` to `Ready`, and back to `Disconnected`.
[on_transport_state_changed()](@ref pipecat::PipecatClientCallbacks::on_transport_state_changed)
reports each change.

## Talking to the bot

Send text with [send_text()](@ref pipecat::PipecatClient::send_text), as if
the user said it, or messages your bot understands with
[send_client_message()](@ref pipecat::PipecatClient::send_client_message).
[send_client_request()](@ref pipecat::PipecatClient::send_client_request)
also waits for the bot's answer, with a callback or a `std::future`:

```cpp
client.send_text("What's the weather like?");

client.send_client_message("set-volume", {{"level", 5}});

// get() throws MessageError if the request fails.
auto weather = client.send_client_request("get-weather", {{"city", "SF"}}).get();
```

Receive what the user and the bot say with callbacks, like
[on_user_transcript()](@ref pipecat::PipecatClientCallbacks::on_user_transcript)
and [on_bot_output()](@ref pipecat::PipecatClientCallbacks::on_bot_output):

```cpp
void on_user_transcript(const pipecat::rtvi::TranscriptData& data) override {
    if (data.final) {
        std::cout << "User: " << data.text << std::endl;
    }
}

void on_bot_output(const pipecat::rtvi::BotOutputData& data) override {
    std::cout << "Bot: " << data.text << std::endl;
}
```

Handle function calls from the bot's LLM in your app with
[register_function_call_handler()](@ref pipecat::PipecatClient::register_function_call_handler):

```cpp
client.register_function_call_handler(
        "move_arm",
        [&](const auto& params, auto respond) {
            // Respond once the arm has moved, without blocking the client.
            robot.move_arm(params.arguments, [respond](bool done) {
                respond({{"done", done}});
            });
        }
);
```

Send the user's audio and play the bot's with
[send_user_audio()](@ref pipecat::PipecatClient::send_user_audio) and
[read_bot_audio()](@ref pipecat::PipecatClient::read_bot_audio), from your
audio threads. Both use 16-bit PCM:

```cpp
// Microphone thread.
client.send_user_audio(mic_frames, num_frames);

// Speaker thread.
int32_t read = client.read_bot_audio(speaker_frames, num_frames);
```

You can also send phone keypad keys with
[send_dtmf()](@ref pipecat::PipecatClient::send_dtmf), and ask the bot to
leave with [disconnect_bot()](@ref pipecat::PipecatClient::disconnect_bot).
