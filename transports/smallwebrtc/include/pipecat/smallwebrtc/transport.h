//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// A transport that connects to Pipecat bots over WebRTC, peer to peer.

#ifndef PIPECAT_SMALLWEBRTC_TRANSPORT_H
#define PIPECAT_SMALLWEBRTC_TRANSPORT_H

#include <pipecat/transport.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pipecat {

/// Options to create a SmallWebRTCTransport.
struct SmallWebRTCTransportOptions {
    /// Sample rate of the user audio you send, in Hz.
    uint32_t user_audio_sample_rate = 16000;
    /// Number of channels of the user audio you send.
    uint8_t user_audio_channels = 1;
    /// Sample rate of the bot audio you read, in Hz.
    uint32_t bot_audio_sample_rate = 16000;
    /// Number of channels of the bot audio you read.
    uint8_t bot_audio_channels = 1;
};

/// Connects to Pipecat bots over WebRTC, peer to peer, with Pipecat's
/// SmallWebRTC transport.
///
/// Give it to PipecatClientOptions::transport. To connect, it sends a WebRTC
/// offer to the bot's server. With PipecatClient::start_bot(), it finds out
/// where on its own: start endpoints like Pipecat's development runner
/// (`python bot.py -t webrtc`) and Pipecat Cloud answer with a session ID when
/// asked for the `webrtc` transport. Without it, give it the offer endpoint,
/// see connect().
///
/// You can use several at the same time, e.g. one per client to talk to
/// several bots.
class SmallWebRTCTransport : public Transport {
   public:
    /// Creates a transport.
    explicit SmallWebRTCTransport(SmallWebRTCTransportOptions options = {});

    /// Disconnects if needed.
    ~SmallWebRTCTransport() override;

    /// Transports can't be copied.
    SmallWebRTCTransport(const SmallWebRTCTransport&) = delete;

    /// Transports can't be copied.
    SmallWebRTCTransport& operator=(const SmallWebRTCTransport&) = delete;

    /// Prepares the transport.
    void initialize(TransportObserver* observer) override;

    /// Keeps the request that started the bot, to send the offer to the same
    /// server.
    void set_start_bot_params(const APIRequest& request) override;

    /// Connects to the bot.
    ///
    /// `params` says where to send the offer, with one of:
    ///
    /// - `sessionId`: the session a start endpoint created, after
    ///   PipecatClient::start_bot(). The offer goes to the start endpoint's
    ///   URL, with `/start` at the end of its path replaced by
    ///   `/sessions/<sessionId>/api/offer`, and with the same headers.
    /// - `webrtcRequestParams`: the offer endpoint, as a JSON object with its
    ///   URL in `endpoint`, and optionally HTTP headers in `headers` and data
    ///   for the bot in `requestData`.
    ///
    /// If both are there, `webrtcRequestParams` is used. Optionally,
    /// `iceConfig` has STUN and TURN servers in `iceServers`, each with its
    /// URLs in `urls`, and `username` and `credential` for TURN. Every key can
    /// also be in snake case, e.g. `session_id`.
    ///
    /// Throws InvalidTransportParamsError if the params are wrong, and
    /// TransportStartError if connecting fails.
    void connect(const nlohmann::json& params) override;

    /// Closes the connection.
    void disconnect() override;

    /// Sends the `client-ready` message.
    void send_ready_message(const rtvi::Message& message) override;

    /// Sends a message to the bot.
    void send_message(const rtvi::Message& message) override;

    /// Sends `num_frames` frames of 16-bit PCM user audio, in the format of
    /// the options. Returns the number of frames sent.
    ///
    /// Send the audio continuously, as it's captured, and send silence while
    /// the user is muted: the bot needs it to tell when the user stops
    /// speaking. The audio is sent as is, without echo cancellation, and in
    /// mono, which is what the bot listens to.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames) override;

    /// Reads up to `num_frames` frames of 16-bit PCM bot audio into `frames`,
    /// converted to the format of the options. Waits until there's audio to
    /// read, or until the transport disconnects. Returns the number of frames
    /// read, or 0 if it disconnected.
    ///
    /// The bot's audio arrives as it plays. The transport keeps up to a
    /// second of it, so read it as you play it.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames) override;

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
