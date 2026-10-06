# SmallWebRTC transport {#smallwebrtc}

The SmallWebRTC transport,
[SmallWebRTCTransport](@ref pipecat::SmallWebRTCTransport), connects the
client to Pipecat bots over WebRTC, peer to peer, with Pipecat's SmallWebRTC
transport. To build it, see its
[README](https://github.com/pipecat-ai/pipecat-client-cxx/tree/main/transports/smallwebrtc).

## Using it

Give a [SmallWebRTCTransport](@ref pipecat::SmallWebRTCTransport) to the
client when you create it:

```cpp
#include <pipecat/pipecat.h>
#include <pipecat/smallwebrtc/transport.h>

pipecat::PipecatClientOptions options;
options.transport = std::make_unique<pipecat::SmallWebRTCTransport>();
options.callbacks = &app;

pipecat::PipecatClient client(std::move(options));
```

Then use the client as usual. Everything else, like callbacks and threads,
works the same with every transport.

## Connecting

The transport sends a WebRTC offer to the bot's server, which answers with
its own, and then connects to the bot directly.

Start endpoints, like Pipecat's development runner
(`python bot.py -t webrtc`) and Pipecat Cloud, start a session when you ask
for the `webrtc` transport, and answer with its ID. The transport then sends
the offer to the same server, with the same headers:

```cpp
pipecat::APIRequest request;
request.endpoint = "http://localhost:7860/start";
request.request_data = {{"transport", "webrtc"}};

client.start_bot_and_connect(request);
```

The offer goes to the start endpoint's URL with `/start` at the end of its
path replaced by `/sessions/<sessionId>/api/offer`.

If your app already knows where to send the offer, e.g. because your backend
started the bot, connect directly with `webrtcRequestParams`. It has the
offer endpoint's URL in `endpoint`, and optionally HTTP headers in `headers`
and data for the bot in `requestData`:

```cpp
client.connect({{"webrtcRequestParams", {
    {"endpoint", "http://localhost:7860/api/offer"},
    {"headers", {{"Authorization", "Bearer " + token}}},
}}});
```

Bots on other networks need STUN or TURN servers to be reached. They go in
`iceConfig`, which start endpoints like Pipecat's development runner return
too. Pipecat's development runner returns the servers it was started with
(`--ice-servers`), or a public STUN server when you add
`{"enableDefaultIceServers", true}` to the request:

```cpp
client.connect({
    {"webrtcRequestParams", {{"endpoint", offer_url}}},
    {"iceConfig", {{"iceServers", {
        {{"urls", "stun:stun.l.google.com:19302"}},
        {{"urls", "turn:turn.example.com:3478"},
         {"username", "user"},
         {"credential", "secret"}},
    }}}},
});
```

Every key can also be in snake case, e.g. `session_id` or `ice_config`.

## Audio

The transport sends the user's audio to the bot and receives the bot's, as
16-bit PCM. Choose the sample rate and channels with
[SmallWebRTCTransportOptions](@ref pipecat::SmallWebRTCTransportOptions).
They default to 16 kHz mono, and any sample rate works:

```cpp
pipecat::SmallWebRTCTransportOptions transport_options;
transport_options.user_audio_sample_rate = 48000;
transport_options.bot_audio_sample_rate = 48000;

options.transport =
        std::make_unique<pipecat::SmallWebRTCTransport>(transport_options);
```

The user's audio is sent to the bot with Opus, in mono, which is what the bot
listens to. The bot's audio is converted to the format of the options.

Send and read audio with the client's `send_user_audio()` and
`read_bot_audio()`, from your audio threads. `read_bot_audio()` waits until
it's time to play the bot's audio. Disconnecting wakes it up, so disconnect
before stopping a thread that reads bot audio.

The bot's audio plays at the pace the bot sent it, 80 ms behind, so the
network's ups and downs don't reach your app: audio that arrives late or out
of order still plays in its turn, and lost audio is concealed. If audio keeps
arriving late, it plays a little further behind, up to 300 ms, and catches up
again during silence.

Send the user's audio continuously, as it's captured, and send silence while
the user is muted: the bot needs it to tell when the user stops speaking.
Until you send some, the transport tells the bot the user's audio is off.

The user's audio is sent as is, without echo cancellation. If your app plays
the bot through speakers, use your platform's echo cancellation or
headphones, or the bot will hear itself.

## Several bots at once

Each client needs its own SmallWebRTCTransport, and you can have several
clients at the same time, e.g. to talk to several bots. A client can also
disconnect and connect again as many times as you need.
