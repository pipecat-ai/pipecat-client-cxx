# WebSocket transport {#websocket}

The WebSocket transport, [WebSocketTransport](@ref pipecat::WebSocketTransport),
connects the client to Pipecat bots over a WebSocket. To build it, see its
[README](https://github.com/pipecat-ai/pipecat-client-cxx/tree/main/transports/websocket).

## Using it

Give a [WebSocketTransport](@ref pipecat::WebSocketTransport) to the client
when you create it:

```cpp
#include <pipecat/pipecat.h>
#include <pipecat/websocket/transport.h>

pipecat::PipecatClientOptions options;
options.transport = std::make_unique<pipecat::WebSocketTransport>();
options.callbacks = &app;

pipecat::PipecatClient client(std::move(options));
```

Then use the client as usual. Everything else, like callbacks and threads,
works the same with every transport.

The bot needs a WebSocket transport with Pipecat's `ProtobufFrameSerializer`,
which the transport uses to talk to it.

## Connecting

The transport connects to the bot's WebSocket. It needs its URL, and a token
if the bot needs one.

Start endpoints, like Pipecat's development runner
(`python bot.py -t websocket`), return both when you ask for the `websocket`
transport, and the client passes them to the transport:

```cpp
pipecat::APIRequest request;
request.endpoint = "http://localhost:7860/start";
request.request_data = {{"transport", "websocket"}};

client.start_bot_and_connect(request);
```

If your app already has them, e.g. because your backend started the bot,
connect directly:

```cpp
client.connect({{"wsUrl", ws_url}, {"token", token}});
```

The URL can be in `wsUrl` or `ws_url`, and the token in `token`.

## Audio

The transport sends the user's audio to the bot and receives the bot's, as
16-bit PCM. Choose the sample rate and channels with
[WebSocketTransportOptions](@ref pipecat::WebSocketTransportOptions). They
default to 16 kHz mono:

```cpp
pipecat::WebSocketTransportOptions transport_options;
transport_options.user_audio_sample_rate = 48000;
transport_options.bot_audio_sample_rate = 48000;

options.transport =
        std::make_unique<pipecat::WebSocketTransport>(transport_options);
```

The bot's audio is converted to these, from whatever sample rate and channels
the bot sends.

Send and read audio with the client's `send_user_audio()` and
`read_bot_audio()`, from your audio threads. `read_bot_audio()` waits until
there's bot audio to read, and the bot only sends audio while it speaks.
Disconnecting wakes it up, so disconnect before stopping a thread that reads
bot audio.

The bot sends its audio faster than it plays, so the transport keeps up to a
minute of it until you read it. Read it as you play it. When the bot is
interrupted, the audio you haven't read yet is dropped.

Send the user's audio continuously, as it's captured, and send silence while
the user is muted: the bot needs it to tell when the user stops speaking.

The user's audio is sent as is, without echo cancellation. If your app plays
the bot through speakers, use your platform's echo cancellation or
headphones, or the bot will hear itself.

## Several bots at once

Each client needs its own WebSocketTransport, and you can have several
clients at the same time, e.g. to talk to several bots. A client can also
disconnect and connect again as many times as you need.
