//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// The interface to write transports, and the connection state.

#ifndef PIPECAT_TRANSPORT_H
#define PIPECAT_TRANSPORT_H

#include "pipecat/rtvi/messages.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace pipecat {

/// The state of the connection to the bot.
enum class TransportState {
    Disconnected,    ///< Not connected.
    Initializing,    ///< The transport is getting ready.
    Initialized,     ///< The transport is ready to connect.
    Authenticating,  ///< Starting the bot.
    Authenticated,   ///< The bot was started.
    Connecting,      ///< Connecting to the bot.
    Connected,       ///< Connected, waiting for the bot to be ready.
    Ready,           ///< The bot is ready.
    Disconnecting,   ///< Disconnecting from the bot.
    Error,           ///< Starting or connecting to the bot failed.
};

/// The name of a state, e.g. "ready".
const char* to_string(TransportState state);

/// Someone in the session, like the bot or another user.
struct Participant {
    /// Unique ID of the participant.
    std::string id;
    /// Display name. Can be empty.
    std::string name;
    /// Whether this is the local user.
    bool local = false;
};

/// Receives events from a transport.
///
/// The client gives one to Transport::initialize(). Transports can call its
/// methods from any thread. They return right away and never block.
class TransportObserver {
   public:
    virtual ~TransportObserver() = default;

    /// A message arrived from the bot. `message` is the RTVI message as JSON.
    virtual void on_transport_message(const nlohmann::json& message) = 0;

    /// The bot joined the session.
    virtual void on_bot_connected(const Participant& bot) = 0;

    /// The bot left the session.
    virtual void on_bot_disconnected(const Participant& bot) = 0;

    /// Someone other than the bot joined the session.
    virtual void on_participant_joined(const Participant& participant) = 0;

    /// Someone other than the bot left the session.
    virtual void on_participant_left(const Participant& participant) = 0;

    /// Something went wrong in the transport. `fatal` is true if the session
    /// can't go on, e.g. because the bot's room was closed.
    virtual void on_transport_error(const std::string& error, bool fatal) = 0;

    /// The transport disconnected on its own, e.g. because the session ended or
    /// the network dropped. Not called after Transport::disconnect().
    virtual void on_transport_disconnected() = 0;
};

/// Connects the client to a bot, e.g. over WebRTC.
///
/// Implement this to add a new transport. Only PipecatClient calls these
/// methods, and never from inside a TransportObserver method, so connect() and
/// disconnect() can wait for the transport's own events. It calls
/// initialize(), connect() and disconnect() one at a time.
class Transport {
   public:
    virtual ~Transport() = default;

    /// Prepares the transport. Called once, before anything else, with the
    /// observer to send events to.
    virtual void initialize(TransportObserver* observer) = 0;

    /// Connects to the bot and returns once connected.
    ///
    /// `params` are the transport's connection parameters, usually what
    /// PipecatClient::start_bot() returned. Throws InvalidTransportParamsError
    /// if they're wrong, and TransportStartError if connecting fails.
    virtual void connect(const nlohmann::json& params) = 0;

    /// Disconnects from the bot and returns once disconnected. Does nothing if
    /// not connected.
    virtual void disconnect() = 0;

    /// Sends the `client-ready` message.
    ///
    /// Send it as soon as the bot can receive it, which may be after connect()
    /// returns (e.g. once the bot's audio starts). You can add details about
    /// the transport to the message.
    virtual void send_ready_message(const rtvi::Message& message) = 0;

    /// Sends a message to the bot. Returns right away.
    virtual void send_message(const rtvi::Message& message) = 0;

    /// The largest message, in bytes, that send_message() accepts.
    virtual size_t max_message_size() const { return 64 * 1024; }

    /// Sends `num_frames` frames of 16-bit PCM user audio to the bot. Returns
    /// the number of frames sent.
    virtual int32_t
    send_user_audio(const int16_t* frames, size_t num_frames) = 0;

    /// Reads up to `num_frames` frames of 16-bit PCM bot audio into `frames`.
    /// Returns the number of frames read.
    virtual int32_t read_bot_audio(int16_t* frames, size_t num_frames) = 0;
};

}  // namespace pipecat

#endif
