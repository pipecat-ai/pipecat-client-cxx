# Overview

The Daily transport connects the
[Pipecat C++ Client SDK](https://github.com/pipecat-ai/pipecat-client-cxx)
to Pipecat bots over [Daily](https://www.daily.co), using WebRTC.

This is its API reference. For the client itself, see the
[Pipecat C++ Client SDK reference](https://docs-cxx.pipecat.ai). To install
the transport and get started, see the
[README](https://github.com/pipecat-ai/pipecat-client-cxx-daily).

## Using it

Give a [DailyTransport](@ref pipecat::DailyTransport) to the client when you
create it:

```cpp
#include <pipecat/daily/transport.h>
#include <pipecat/pipecat.h>

pipecat::PipecatClientOptions options;
options.transport = std::make_unique<pipecat::DailyTransport>();
options.callbacks = &app;

pipecat::PipecatClient client(std::move(options));
```

Then use the client as usual. Everything else, like callbacks and threads,
works the same with every transport.

## Connecting

The transport joins the bot's Daily room. It needs the room URL, and a token
if the room needs one.

Start endpoints, like Pipecat Cloud or Pipecat's development runner, can
create the room when they start the bot. Ask for one with `createDailyRoom`,
and the client passes the room they answer with to the transport:

```cpp
pipecat::APIRequest request;
request.endpoint = "https://api.pipecat.daily.co/v1/public/AGENT/start";
request.headers = {{"Authorization", "Bearer " + api_key}};
request.request_data = {{"createDailyRoom", true}};

client.start_bot_and_connect(request);
```

If your app already has the room, e.g. because your backend started the bot,
connect to it directly:

```cpp
client.connect({{"url", room_url}, {"token", token}});
```

The room URL can be in `url`, `dailyRoom` or `room_url`, and the token in
`token` or `dailyToken`.

## Audio

The transport sends the user's audio to the bot and receives the bot's, as
16-bit PCM. Choose the sample rate and channels with
[DailyTransportOptions](@ref pipecat::DailyTransportOptions). They default to
16 kHz mono:

```cpp
pipecat::DailyTransportOptions transport_options;
transport_options.user_audio_sample_rate = 48000;
transport_options.bot_audio_sample_rate = 48000;

options.transport =
        std::make_unique<pipecat::DailyTransport>(transport_options);
```

Send and read audio with the client's `send_user_audio()` and
`read_bot_audio()`, from your audio threads. `read_bot_audio()` waits until
there's bot audio to read. Disconnecting wakes it up, so disconnect before
stopping a thread that reads bot audio.

Send the user's audio continuously, as it's captured, and send silence while
the user is muted: the bot needs it to tell when the user stops speaking.

The user's audio is sent as is, without echo cancellation. If your app plays
the bot through speakers, use your platform's echo cancellation or
headphones, or the bot will hear itself.

## Several bots at once

Each client needs its own DailyTransport, and you can have several clients at
the same time, e.g. to talk to several bots. A client can also disconnect and
connect again as many times as you need.
