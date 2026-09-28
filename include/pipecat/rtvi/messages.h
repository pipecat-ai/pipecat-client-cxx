//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_RTVI_MESSAGES_H
#define PIPECAT_RTVI_MESSAGES_H

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// RTVI protocol messages and the data they carry.
//
// Parsing is lenient: missing or null fields get default values (or
// std::nullopt when optional), so older or newer servers don't break parsing.
// A field with the wrong JSON type throws nlohmann::json::exception.

namespace pipecat::rtvi {

// RTVI protocol version implemented by this library.
inline constexpr const char* PROTOCOL_VERSION = "2.1.0";

// Label carried by every RTVI message.
inline constexpr const char* MESSAGE_LABEL = "rtvi-ai";

enum class MessageType {
    // Client to server.
    ClientReady,
    DisconnectBot,
    ClientMessage,
    SendText,
    DTMF,
    LLMFunctionCallResult,

    // Server to client.
    BotReady,
    Error,
    ErrorResponse,
    ServerMessage,
    ServerResponse,
    Metrics,
    UserStartedSpeaking,
    UserStoppedSpeaking,
    BotStartedSpeaking,
    BotStoppedSpeaking,
    UserMuteStarted,
    UserMuteStopped,
    UserTranscription,
    UserLLMText,
    BotOutput,
    BotLLMText,
    BotLLMStarted,
    BotLLMStopped,
    BotLLMSearchResponse,
    BotTTSText,
    BotTTSStarted,
    BotTTSStopped,
    LLMFunctionCallStarted,
    LLMFunctionCallInProgress,
    LLMFunctionCallStopped,
};

// Wire name of a message type (e.g. "bot-ready").
const char* to_string(MessageType type);

// Message type for a wire name, or std::nullopt if unknown.
std::optional<MessageType> parse_message_type(const std::string& type);

//
// Client to server data.
//

// Information about the client, sent to the server in `client-ready`.
struct AboutClientData {
    std::string library;
    std::optional<std::string> library_version;
    std::optional<std::string> platform;
    std::optional<std::string> platform_version;
    // An object with any extra details, or null to omit it.
    nlohmann::json platform_details;
};

// About data describing this library and the platform it runs on.
AboutClientData default_about_client();

// App-defined message: `client-message` to the server and `server-response`
// back. Named `t` and `d` on the wire.
struct ClientMessageData {
    std::string type;
    nlohmann::json data;
};

struct SendTextOptions {
    // Whether the bot should respond right away. Server default: true.
    std::optional<bool> run_immediately;
    // Whether the bot should respond with audio. Server default: true.
    std::optional<bool> audio_response;
};

struct LLMFunctionCallResultData {
    std::string function_name;
    std::string tool_call_id;
    nlohmann::json arguments;
    nlohmann::json result;
};

//
// Server to client data.
//

struct BotReadyData {
    std::string version;
    // Optional information about the bot, or null.
    nlohmann::json about;
};

// Used by both `error` and `error-response` messages. `fatal` is always false
// for `error-response`.
struct ErrorData {
    std::string error;
    bool fatal = false;
};

struct TranscriptData {
    std::string text;
    bool final = false;
    std::string timestamp;
    std::string user_id;
};

struct TextData {
    std::string text;
};

using UserLLMTextData = TextData;
using BotLLMTextData = TextData;
using BotTTSTextData = TextData;

struct SpokenProgressData {
    // Text already spoken in this segment, including the current word.
    std::string accumulated_text;
    // Text not yet spoken in this segment.
    std::string remaining_text;
};

// A best-effort aggregation of what the bot says or outputs.
struct BotOutputData {
    std::string text;
    // What the text is: "word", "sentence" or a custom aggregation.
    std::string aggregated_by;
    std::optional<int64_t> segment_id;
    // Whether the text will be spoken by TTS.
    std::optional<bool> will_be_spoken;
    // "new", "in-progress" or "completed".
    std::optional<std::string> spoken_status;
    // Present when the text will be spoken.
    std::optional<SpokenProgressData> spoken_progress;
};

struct MetricData {
    std::string processor;
    std::optional<std::string> model;
    double value = 0;
};

struct MetricsData {
    std::vector<MetricData> ttfb;
    std::vector<MetricData> processing;
    std::vector<MetricData> characters;
    // The full metrics data, including metrics without a typed field above
    // (e.g. "ttfa" or "tokens").
    nlohmann::json raw;
};

struct LLMFunctionCallStartedData {
    // Only present if the server is configured to report it.
    std::optional<std::string> function_name;
};

struct LLMFunctionCallInProgressData {
    std::optional<std::string> function_name;
    std::string tool_call_id;
    // An object, empty if the server didn't send the arguments.
    nlohmann::json arguments;
};

struct LLMFunctionCallStoppedData {
    std::optional<std::string> function_name;
    std::string tool_call_id;
    bool cancelled = false;
    // The function result, or null.
    nlohmann::json result;
};

struct LLMSearchResult {
    std::string text;
    std::vector<double> confidence;
};

struct LLMSearchOrigin {
    std::optional<std::string> site_uri;
    std::optional<std::string> site_title;
    std::vector<LLMSearchResult> results;
};

struct BotLLMSearchResponseData {
    std::optional<std::string> search_result;
    std::optional<std::string> rendered_content;
    std::vector<LLMSearchOrigin> origins;
};

//
// Messages.
//

// An RTVI message. `type` is kept as a string so unknown or custom types can
// still be represented. Use parse_message_type() to get a MessageType.
struct Message {
    std::string id;
    std::string label = MESSAGE_LABEL;
    std::string type;
    // Null if the message has no data.
    nlohmann::json data;

    Message() = default;

    // Creates a message with a new random id.
    explicit Message(MessageType type, nlohmann::json data = nullptr);

    static Message client_ready(const AboutClientData& about);

    static Message disconnect_bot();

    static Message
    client_message(const std::string& type, const nlohmann::json& data);

    static Message
    send_text(const std::string& content, const SendTextOptions& options = {});

    // One or more DTMF keys (0-9, * and #), in order. Needs protocol 2.1.0+.
    static Message dtmf(const std::string& buttons);

    static Message llm_function_call_result(
            const LLMFunctionCallResultData& data
    );
};

//
// JSON conversions, used by nlohmann::json (e.g. `data.get<BotOutputData>()`).
//

void to_json(nlohmann::json& j, const Message& message);
void from_json(const nlohmann::json& j, Message& message);

void to_json(nlohmann::json& j, const AboutClientData& data);
void to_json(nlohmann::json& j, const SendTextOptions& options);
void to_json(nlohmann::json& j, const LLMFunctionCallResultData& data);

void to_json(nlohmann::json& j, const ClientMessageData& data);
void from_json(const nlohmann::json& j, ClientMessageData& data);

void from_json(const nlohmann::json& j, BotReadyData& data);
void from_json(const nlohmann::json& j, ErrorData& data);
void from_json(const nlohmann::json& j, TranscriptData& data);
void from_json(const nlohmann::json& j, TextData& data);
void from_json(const nlohmann::json& j, SpokenProgressData& data);
void from_json(const nlohmann::json& j, BotOutputData& data);
void from_json(const nlohmann::json& j, MetricData& data);
void from_json(const nlohmann::json& j, MetricsData& data);
void from_json(const nlohmann::json& j, LLMFunctionCallStartedData& data);
void from_json(const nlohmann::json& j, LLMFunctionCallInProgressData& data);
void from_json(const nlohmann::json& j, LLMFunctionCallStoppedData& data);
void from_json(const nlohmann::json& j, LLMSearchResult& data);
void from_json(const nlohmann::json& j, LLMSearchOrigin& data);
void from_json(const nlohmann::json& j, BotLLMSearchResponseData& data);

}  // namespace pipecat::rtvi

#endif
