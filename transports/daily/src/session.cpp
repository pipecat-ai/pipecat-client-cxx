//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "session.h"

#include <pipecat/errors.h>

#include <utility>

using nlohmann::json;

namespace pipecat::daily {

namespace {

// String value of `key`, or "" if it's missing or not a string.
std::string string_value(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}

// First non-empty string value of `keys`.
std::string
first_string_value(const json& j, std::initializer_list<const char*> keys) {
    for (const char* key: keys) {
        std::string value = string_value(j, key);
        if (!value.empty()) {
            return value;
        }
    }
    return "";
}

const json& object_value(const json& j, const char* key) {
    static const json EMPTY = json::object();
    auto it = j.find(key);
    return it != j.end() && it->is_object() ? *it : EMPTY;
}

bool is_local(const json& participant) {
    const json& info = object_value(participant, "info");
    auto it = info.find("isLocal");
    return it != info.end() && it->is_boolean() && it->get<bool>();
}

bool is_audio_playable(const json& participant) {
    const json& microphone =
            object_value(object_value(participant, "media"), "microphone");
    return string_value(microphone, "state") == "playable";
}

Participant to_participant(const json& participant) {
    Participant result;
    result.id = string_value(participant, "id");
    result.name = string_value(object_value(participant, "info"), "userName");
    result.local = is_local(participant);
    return result;
}

}  // namespace

ConnectionParams parse_connection_params(const json& params) {
    if (!params.is_object()) {
        throw InvalidTransportParamsError(
                "Daily connection params must be a JSON object"
        );
    }

    ConnectionParams result;
    result.url = first_string_value(params, {"url", "dailyRoom", "room_url"});
    result.token = first_string_value(params, {"token", "dailyToken"});
    if (result.url.empty()) {
        throw InvalidTransportParamsError(
                "Daily connection params need the room URL, in `url`, "
                "`dailyRoom` or `room_url`"
        );
    }
    return result;
}

Session::Session(TransportObserver* observer, SendAppMessage send_app_message)
    : _observer(observer), _send_app_message(std::move(send_app_message)) {}

void Session::send_ready_message(std::string message) {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_bot_audio_playable) {
            _ready_message = std::move(message);
            return;
        }
    }
    _send_app_message(std::move(message));
}

void Session::set_leaving() {
    std::lock_guard<std::mutex> lock(_mutex);
    _leaving = true;
}

bool Session::left() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _left;
}

void Session::handle_event(const json& event) {
    std::string action = string_value(event, "action");
    if (action == "participant-joined") {
        handle_participant(object_value(event, "participant"), true);
    } else if (action == "participant-updated") {
        handle_participant(object_value(event, "participant"), false);
    } else if (action == "participant-left") {
        handle_participant_left(object_value(event, "participant"));
    } else if (action == "app-message") {
        handle_app_message(event);
    } else if (action == "call-state-updated") {
        handle_call_state(event);
    } else if (action == "error") {
        handle_error(event);
    }
}

void Session::handle_participant(const json& participant, bool joined) {
    Participant info = to_participant(participant);
    if (info.local || info.id.empty()) {
        return;
    }

    bool new_bot = false;
    bool new_participant = false;
    std::optional<std::string> ready_message;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_leaving) {
            return;
        }
        if (!_bot) {
            _bot = info;
            new_bot = true;
        } else if (joined && _bot->id != info.id) {
            new_participant = true;
        }
        if (_bot->id == info.id && is_audio_playable(participant) &&
            !_bot_audio_playable) {
            _bot_audio_playable = true;
            ready_message = std::exchange(_ready_message, std::nullopt);
        }
    }

    if (new_bot) {
        _observer->on_bot_connected(info);
    }
    if (new_participant) {
        _observer->on_participant_joined(info);
    }
    if (ready_message) {
        _send_app_message(std::move(*ready_message));
    }
}

void Session::handle_participant_left(const json& participant) {
    Participant info = to_participant(participant);
    bool bot_left = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_leaving || info.local || info.id.empty()) {
            return;
        }
        if (_bot && _bot->id == info.id) {
            _bot.reset();
            _bot_audio_playable = false;
            bot_left = true;
        }
    }

    if (bot_left) {
        _observer->on_bot_disconnected(info);
    } else {
        _observer->on_participant_left(info);
    }
}

void Session::handle_app_message(const json& event) {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_leaving) {
            return;
        }
    }

    const json& message = object_value(event, "msgData");
    if (string_value(message, "label") == rtvi::MESSAGE_LABEL) {
        _observer->on_transport_message(message);
    }
}

void Session::handle_call_state(const json& event) {
    if (string_value(event, "state") != "left") {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_leaving || _left) {
            return;
        }
        _left = true;
    }

    std::string error = string_value(event, "error");
    if (!error.empty()) {
        _observer->on_transport_error(error, true);
    }
    _observer->on_transport_disconnected();
}

void Session::handle_error(const json& event) {
    std::string error = string_value(event, "message");
    _observer->on_transport_error(
            error.empty() ? "Unknown Daily error" : error, true
    );
}

}  // namespace pipecat::daily
