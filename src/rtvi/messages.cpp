//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/rtvi/messages.h"

#include <random>
#include <unordered_map>

using nlohmann::json;

namespace pipecat::rtvi {

namespace {

const size_t ID_LENGTH = 10;

std::string generate_id() {
    static const char CHARACTERS[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    thread_local std::mt19937 generator(std::random_device {}());
    std::uniform_int_distribution<size_t> distribution(
            0, sizeof(CHARACTERS) - 2
    );

    std::string id(ID_LENGTH, '\0');
    for (char& c: id) {
        c = CHARACTERS[distribution(generator)];
    }
    return id;
}

// Value of `key`, or `fallback` if it's missing or null.
template<typename T>
T value_or(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return fallback;
    }
    return it->get<T>();
}

// Value of `key`, or std::nullopt if it's missing or null.
template<typename T>
std::optional<T> optional_value(const json& j, const char* key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        return std::nullopt;
    }
    return it->get<T>();
}

// Raw JSON of `key`, or null if it's missing.
json json_value(const json& j, const char* key) {
    auto it = j.find(key);
    return it == j.end() ? json(nullptr) : *it;
}

template<typename T>
void set_optional(json& j, const char* key, const std::optional<T>& value) {
    if (value) {
        j[key] = *value;
    }
}

}  // namespace

const char* to_string(MessageType type) {
    switch (type) {
    case MessageType::ClientReady:
        return "client-ready";
    case MessageType::DisconnectBot:
        return "disconnect-bot";
    case MessageType::ClientMessage:
        return "client-message";
    case MessageType::SendText:
        return "send-text";
    case MessageType::DTMF:
        return "dtmf";
    case MessageType::LLMFunctionCallResult:
        return "llm-function-call-result";
    case MessageType::BotReady:
        return "bot-ready";
    case MessageType::Error:
        return "error";
    case MessageType::ErrorResponse:
        return "error-response";
    case MessageType::ServerMessage:
        return "server-message";
    case MessageType::ServerResponse:
        return "server-response";
    case MessageType::Metrics:
        return "metrics";
    case MessageType::UserStartedSpeaking:
        return "user-started-speaking";
    case MessageType::UserStoppedSpeaking:
        return "user-stopped-speaking";
    case MessageType::BotStartedSpeaking:
        return "bot-started-speaking";
    case MessageType::BotStoppedSpeaking:
        return "bot-stopped-speaking";
    case MessageType::UserMuteStarted:
        return "user-mute-started";
    case MessageType::UserMuteStopped:
        return "user-mute-stopped";
    case MessageType::UserTranscription:
        return "user-transcription";
    case MessageType::UserLLMText:
        return "user-llm-text";
    case MessageType::BotOutput:
        return "bot-output";
    case MessageType::BotTranscription:
        return "bot-transcription";
    case MessageType::BotLLMText:
        return "bot-llm-text";
    case MessageType::BotLLMStarted:
        return "bot-llm-started";
    case MessageType::BotLLMStopped:
        return "bot-llm-stopped";
    case MessageType::BotLLMSearchResponse:
        return "bot-llm-search-response";
    case MessageType::BotTTSText:
        return "bot-tts-text";
    case MessageType::BotTTSStarted:
        return "bot-tts-started";
    case MessageType::BotTTSStopped:
        return "bot-tts-stopped";
    case MessageType::LLMFunctionCall:
        return "llm-function-call";
    case MessageType::LLMFunctionCallStarted:
        return "llm-function-call-started";
    case MessageType::LLMFunctionCallInProgress:
        return "llm-function-call-in-progress";
    case MessageType::LLMFunctionCallStopped:
        return "llm-function-call-stopped";
    }
    return "";
}

std::optional<MessageType> parse_message_type(const std::string& type) {
    // MessageType values are contiguous from 0, so we can walk them until
    // to_string() doesn't know the value.
    static const auto TYPES = [] {
        std::unordered_map<std::string, MessageType> types;
        for (int i = 0;; i++) {
            auto type = static_cast<MessageType>(i);
            const char* name = to_string(type);
            if (*name == '\0') {
                break;
            }
            types.emplace(name, type);
        }
        return types;
    }();

    auto it = TYPES.find(type);
    if (it == TYPES.end()) {
        return std::nullopt;
    }
    return it->second;
}

//
// Messages
//

Message::Message(MessageType type, json data)
    : id(generate_id()), type(to_string(type)), data(std::move(data)) {}

Message Message::client_ready(const AboutClientData& about) {
    return Message(
            MessageType::ClientReady,
            {{"version", PROTOCOL_VERSION}, {"about", about}}
    );
}

Message Message::disconnect_bot() {
    return Message(MessageType::DisconnectBot, json::object());
}

Message Message::client_message(const std::string& type, const json& data) {
    return Message(MessageType::ClientMessage, ClientMessageData {type, data});
}

Message
Message::send_text(const std::string& content, const SendTextOptions& options) {
    return Message(
            MessageType::SendText, {{"content", content}, {"options", options}}
    );
}

Message Message::dtmf(const std::string& buttons) {
    json array = json::array();
    for (char button: buttons) {
        array.push_back(std::string(1, button));
    }
    return Message(MessageType::DTMF, {{"buttons", array}});
}

Message Message::dtmf_button(char button) {
    return Message(MessageType::DTMF, {{"button", std::string(1, button)}});
}

Message Message::llm_function_call_result(const LLMFunctionCallResultData& data
) {
    return Message(MessageType::LLMFunctionCallResult, data);
}

void to_json(json& j, const Message& message) {
    j = json {
            {"id", message.id},
            {"label", message.label},
            {"type", message.type},
    };
    if (!message.data.is_null()) {
        j["data"] = message.data;
    }
}

void from_json(const json& j, Message& message) {
    message.id = value_or<std::string>(j, "id", "");
    message.label = value_or<std::string>(j, "label", "");
    message.type = value_or<std::string>(j, "type", "");
    message.data = json_value(j, "data");
}

//
// Client to server data
//

void to_json(json& j, const AboutClientData& data) {
    j = json {{"library", data.library}};
    set_optional(j, "library_version", data.library_version);
    set_optional(j, "platform", data.platform);
    set_optional(j, "platform_version", data.platform_version);
    if (!data.platform_details.is_null()) {
        j["platform_details"] = data.platform_details;
    }
}

void to_json(json& j, const SendTextOptions& options) {
    j = json::object();
    set_optional(j, "run_immediately", options.run_immediately);
    set_optional(j, "audio_response", options.audio_response);
}

void to_json(json& j, const LLMFunctionCallResultData& data) {
    j = json {
            {"function_name", data.function_name},
            {"tool_call_id", data.tool_call_id},
            {"arguments",
             data.arguments.is_null() ? json::object() : data.arguments},
            {"result", data.result},
    };
}

void to_json(json& j, const ClientMessageData& data) {
    j = json {{"t", data.type}};
    if (!data.data.is_null()) {
        j["d"] = data.data;
    }
}

void from_json(const json& j, ClientMessageData& data) {
    data.type = value_or<std::string>(j, "t", "");
    data.data = json_value(j, "d");
}

//
// Server to client data
//

void from_json(const json& j, BotReadyData& data) {
    data.version = value_or<std::string>(j, "version", "");
    data.about = json_value(j, "about");
}

void from_json(const json& j, ErrorData& data) {
    // Older servers send the error in `message`.
    data.error = value_or<std::string>(
            j, "error", value_or<std::string>(j, "message", "")
    );
    data.fatal = value_or(j, "fatal", false);
}

void from_json(const json& j, TranscriptData& data) {
    data.text = value_or<std::string>(j, "text", "");
    data.final = value_or(j, "final", false);
    data.timestamp = value_or<std::string>(j, "timestamp", "");
    data.user_id = value_or<std::string>(j, "user_id", "");
}

void from_json(const json& j, TextData& data) {
    data.text = value_or<std::string>(j, "text", "");
}

void from_json(const json& j, SpokenProgressData& data) {
    data.accumulated_text = value_or<std::string>(j, "accumulated_text", "");
    data.remaining_text = value_or<std::string>(j, "remaining_text", "");
}

void from_json(const json& j, BotOutputData& data) {
    data.text = value_or<std::string>(j, "text", "");
    data.aggregated_by = value_or<std::string>(j, "aggregated_by", "");
    data.segment_id = optional_value<int64_t>(j, "segment_id");
    data.spoken = optional_value<bool>(j, "spoken");
    data.will_be_spoken = optional_value<bool>(j, "will_be_spoken");
    data.spoken_status = optional_value<std::string>(j, "spoken_status");
    data.spoken_progress =
            optional_value<SpokenProgressData>(j, "spoken_progress");
}

void from_json(const json& j, MetricData& data) {
    data.processor = value_or<std::string>(j, "processor", "");
    data.model = optional_value<std::string>(j, "model");
    data.value = value_or(j, "value", 0.0);
}

void from_json(const json& j, MetricsData& data) {
    data.ttfb = value_or<std::vector<MetricData>>(j, "ttfb", {});
    data.processing = value_or<std::vector<MetricData>>(j, "processing", {});
    data.characters = value_or<std::vector<MetricData>>(j, "characters", {});
    data.raw = j;
}

void from_json(const json& j, LLMFunctionCallStartedData& data) {
    data.function_name = optional_value<std::string>(j, "function_name");
}

void from_json(const json& j, LLMFunctionCallInProgressData& data) {
    data.function_name = optional_value<std::string>(j, "function_name");
    data.tool_call_id = value_or<std::string>(j, "tool_call_id", "");
    data.arguments = json_value(j, "arguments");
    if (data.arguments.is_null()) {
        // Deprecated `llm-function-call` messages.
        data.arguments = json_value(j, "args");
    }
    if (data.arguments.is_null()) {
        data.arguments = json::object();
    }
}

void from_json(const json& j, LLMFunctionCallStoppedData& data) {
    data.function_name = optional_value<std::string>(j, "function_name");
    data.tool_call_id = value_or<std::string>(j, "tool_call_id", "");
    data.cancelled = value_or(j, "cancelled", false);
    data.result = json_value(j, "result");
}

void from_json(const json& j, LLMSearchResult& data) {
    data.text = value_or<std::string>(j, "text", "");
    data.confidence = value_or<std::vector<double>>(j, "confidence", {});
}

void from_json(const json& j, LLMSearchOrigin& data) {
    data.site_uri = optional_value<std::string>(j, "site_uri");
    data.site_title = optional_value<std::string>(j, "site_title");
    data.results = value_or<std::vector<LLMSearchResult>>(j, "results", {});
}

void from_json(const json& j, BotLLMSearchResponseData& data) {
    data.search_result = optional_value<std::string>(j, "search_result");
    data.rendered_content = optional_value<std::string>(j, "rendered_content");
    data.origins = value_or<std::vector<LLMSearchOrigin>>(j, "origins", {});
}

}  // namespace pipecat::rtvi
