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
    SmallWebRTCTransport();

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

    /// Audio isn't supported yet: sends nothing and returns 0.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames) override;

    /// Audio isn't supported yet: reads nothing and returns 0.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames) override;

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
