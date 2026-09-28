//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_DAILY_SESSION_H
#define PIPECAT_DAILY_SESSION_H

#include <pipecat/transport.h>

#include <nlohmann/json.hpp>

#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace pipecat::daily {

struct ConnectionParams {
    std::string url;
    // Empty if the room doesn't need one.
    std::string token;
};

// Reads the room URL and token, accepting the names used by start endpoints
// and client-js. Throws InvalidTransportParamsError.
ConnectionParams parse_connection_params(const nlohmann::json& params);

// Turns the daily-core events of one session into TransportObserver events.
// Thread-safe. The observer is never called with the session's lock held.
class Session {
   public:
    using SendAppMessage = std::function<void(std::string message)>;

    Session(TransportObserver* observer, SendAppMessage send_app_message);

    // Sends the client-ready message once the bot's audio is playable, or now
    // if it already is.
    void send_ready_message(std::string message);

    // Called before leaving, so the session doesn't report leaving as an
    // unexpected disconnection.
    void set_leaving();

    // Whether the call ended without being asked to.
    bool left() const;

    // Handles a daily-core event, except request completions.
    void handle_event(const nlohmann::json& event);

   private:
    void handle_participant(const nlohmann::json& participant, bool joined);
    void handle_participant_left(const nlohmann::json& participant);
    void handle_app_message(const nlohmann::json& event);
    void handle_call_state(const nlohmann::json& event);
    void handle_error(const nlohmann::json& event);

    TransportObserver* _observer;
    SendAppMessage _send_app_message;

    mutable std::mutex _mutex;
    // The first remote participant is the bot.
    std::optional<Participant> _bot;
    bool _bot_audio_playable = false;
    std::optional<std::string> _ready_message;
    bool _leaving = false;
    bool _left = false;
};

}  // namespace pipecat::daily

#endif
