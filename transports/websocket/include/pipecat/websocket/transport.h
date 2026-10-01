//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// A transport that connects to Pipecat bots over a WebSocket.

#ifndef PIPECAT_WEBSOCKET_TRANSPORT_H
#define PIPECAT_WEBSOCKET_TRANSPORT_H

#include <pipecat/transport.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pipecat {

/// Options to create a WebSocketTransport.
struct WebSocketTransportOptions {
    /// Sample rate of the user audio you send, in Hz.
    uint32_t user_audio_sample_rate = 16000;
    /// Number of channels of the user audio you send.
    uint8_t user_audio_channels = 1;
    /// Sample rate of the bot audio you read, in Hz.
    uint32_t bot_audio_sample_rate = 16000;
    /// Number of channels of the bot audio you read.
    uint8_t bot_audio_channels = 1;
};

/// Connects to Pipecat bots over a WebSocket.
///
/// Give it to PipecatClientOptions::transport. To connect, it needs the URL of
/// the bot's WebSocket, and a token if the bot needs one. Start endpoints like
/// Pipecat's development runner (`python bot.py -t websocket`) return both, as
/// `wsUrl` and `token`, when asked for the `websocket` transport.
///
/// You can use several at the same time, e.g. one per client to talk to
/// several bots.
class WebSocketTransport : public Transport {
   public:
    /// Creates a transport.
    explicit WebSocketTransport(WebSocketTransportOptions options = {});

    /// Disconnects if needed.
    ~WebSocketTransport() override;

    /// Transports can't be copied.
    WebSocketTransport(const WebSocketTransport&) = delete;

    /// Transports can't be copied.
    WebSocketTransport& operator=(const WebSocketTransport&) = delete;

    /// Prepares the transport.
    void initialize(TransportObserver* observer) override;

    /// Connects to the bot's WebSocket.
    ///
    /// `params` has the URL in `wsUrl` (or `ws_url`), and the token, if the bot
    /// needs one, in `token`. Throws InvalidTransportParamsError if the URL is
    /// missing or isn't a WebSocket URL, and TransportStartError if connecting
    /// fails.
    void connect(const nlohmann::json& params) override;

    /// Closes the WebSocket.
    void disconnect() override;

    /// Sends the `client-ready` message.
    void send_ready_message(const rtvi::Message& message) override;

    /// Sends a message to the bot.
    void send_message(const rtvi::Message& message) override;

    /// The largest message the bot accepts, about 1 MB.
    size_t max_message_size() const override;

    /// Sends `num_frames` frames of 16-bit PCM user audio, in the format of
    /// the options. Returns the number of frames sent.
    ///
    /// Send the audio continuously, as it's captured, and send silence while
    /// the user is muted: the bot needs it to tell when the user stops
    /// speaking. The audio is sent as is, without echo cancellation.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames) override;

    /// Reads up to `num_frames` frames of 16-bit PCM bot audio into `frames`,
    /// converted to the format of the options. Waits until there's audio to
    /// read, or until the transport disconnects. Returns the number of frames
    /// read, or 0 if it disconnected.
    ///
    /// The bot only sends audio while it speaks, and sends it faster than it
    /// plays. The transport keeps up to a minute of it, and drops what's left
    /// when the bot is interrupted, so read it as you play it.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames) override;

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
