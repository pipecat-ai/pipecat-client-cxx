//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_TRANSPORT_H
#define PIPECAT_TRANSPORT_H

#include "pipecat/rtvi/messages.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace pipecat {

enum class TransportState {
    Disconnected,
    Initializing,
    Initialized,
    Authenticating,
    Authenticated,
    Connecting,
    Connected,
    Ready,
    Disconnecting,
    Error,
};

// Name of a state (e.g. "ready").
const char* to_string(TransportState state);

struct Participant {
    std::string id;
    std::string name;
    bool local = false;
};

// Events a transport reports to the client. The client handles them without
// blocking, so transports can call these from any thread, including event
// threads that must never block (like daily-core's).
class TransportObserver {
   public:
    virtual ~TransportObserver() = default;

    // An RTVI message arrived from the bot.
    virtual void on_transport_message(const nlohmann::json& message) = 0;

    virtual void on_bot_connected(const Participant& bot) = 0;
    virtual void on_bot_disconnected(const Participant& bot) = 0;

    // A participant other than the bot joined or left.
    virtual void on_participant_joined(const Participant& participant) = 0;
    virtual void on_participant_left(const Participant& participant) = 0;

    // The transport disconnected on its own, e.g. the session ended or the
    // network dropped. Not called for disconnect().
    virtual void on_transport_disconnected() = 0;
};

// Carries messages and audio between the client and a bot, e.g. over WebRTC.
//
// Only PipecatClient calls these methods. It never calls them from a
// TransportObserver method, so they can block while waiting for events.
class Transport {
   public:
    virtual ~Transport() = default;

    // Called once, before any other method, with the observer to report
    // events to.
    virtual void initialize(TransportObserver* observer) = 0;

    // Connects with transport-specific parameters, usually the response of
    // PipecatClient::start_bot(). Blocks until connected. Throws
    // InvalidTransportParamsError or TransportStartError.
    virtual void connect(const nlohmann::json& params) = 0;

    // Blocks until disconnected. Does nothing if not connected.
    virtual void disconnect() = 0;

    // Sends the client-ready message once the bot can receive it, which may
    // be after connect() returns (e.g. when the bot's audio starts playing).
    // Transports can add their own details to the message's `about` data.
    virtual void send_ready_message(const rtvi::Message& message) = 0;

    // Sends a message to the bot without blocking.
    virtual void send_message(const rtvi::Message& message) = 0;

    // Largest serialized message, in bytes, that send_message() accepts.
    virtual size_t max_message_size() const { return 64 * 1024; }

    // Sends 16-bit PCM user audio. Returns the number of frames sent.
    virtual int32_t
    send_user_audio(const int16_t* frames, size_t num_frames) = 0;

    // Reads 16-bit PCM bot audio into `frames`. Returns the number of frames
    // read.
    virtual int32_t read_bot_audio(int16_t* frames, size_t num_frames) = 0;
};

}  // namespace pipecat

#endif
