//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// RTVI messages and their data.

#ifndef PIPECAT_RTVI_MESSAGES_H
#define PIPECAT_RTVI_MESSAGES_H

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// RTVI messages and their data.
///
/// RTVI is the protocol clients and Pipecat bots use to talk to each other.
/// PipecatClient sends messages for you and passes the data of the messages
/// it receives to PipecatClientCallbacks.
///
/// Missing fields in messages from the bot get default values, or
/// std::nullopt if they're optional. All the data types convert to and from
/// nlohmann::json, e.g. `message.data.get<BotOutputData>()`.
namespace pipecat::rtvi {

/// The RTVI protocol version this library speaks.
inline constexpr const char* PROTOCOL_VERSION = "2.1.0";

/// The label of every RTVI message.
inline constexpr const char* MESSAGE_LABEL = "rtvi-ai";

/// Types of RTVI messages.
enum class MessageType {
    // Client to server.
    ClientReady,            ///< The client is ready.
    DisconnectBot,          ///< Asks the bot to leave.
    ClientMessage,          ///< An app-defined message to the bot.
    SendText,               ///< Text for the bot's LLM.
    DTMF,                   ///< DTMF keys.
    LLMFunctionCallResult,  ///< The result of a function call.

    // Server to client.
    BotReady,                   ///< The bot is ready.
    Error,                      ///< An error in the bot.
    ErrorResponse,              ///< The bot couldn't handle a message.
    ServerMessage,              ///< An app-defined message from the bot.
    ServerResponse,             ///< The bot's answer to a request.
    Metrics,                    ///< Performance metrics.
    UserStartedSpeaking,        ///< The user started speaking.
    UserStoppedSpeaking,        ///< The user stopped speaking.
    BotStartedSpeaking,         ///< The bot started speaking.
    BotStoppedSpeaking,         ///< The bot stopped speaking.
    UserMuteStarted,            ///< The bot started ignoring user audio.
    UserMuteStopped,            ///< The bot stopped ignoring user audio.
    UserTranscription,          ///< What the user said.
    UserLLMText,                ///< User text sent to the bot's LLM.
    BotOutput,                  ///< What the bot says or outputs.
    BotLLMText,                 ///< Text from the bot's LLM, as it streams.
    BotLLMStarted,              ///< The bot's LLM started responding.
    BotLLMStopped,              ///< The bot's LLM finished responding.
    BotLLMSearchResponse,       ///< Search results used by the bot's LLM.
    BotTTSText,                 ///< Text the bot is speaking.
    BotTTSStarted,              ///< The bot's text-to-speech started.
    BotTTSStopped,              ///< The bot's text-to-speech stopped.
    LLMFunctionCallStarted,     ///< The bot's LLM started a function call.
    LLMFunctionCallInProgress,  ///< A function call is running.
    LLMFunctionCallStopped,     ///< A function call finished.
};

/// The RTVI name of a message type, e.g. "bot-ready".
const char* to_string(MessageType type);

/// The message type with an RTVI name, or std::nullopt if it's unknown.
std::optional<MessageType> parse_message_type(const std::string& type);

/// Information about the client, sent to the bot when connecting.
struct AboutClientData {
    /// Name of the library, e.g. "pipecat-client-cxx".
    std::string library;
    /// Version of the library.
    std::optional<std::string> library_version;
    /// Operating system, e.g. "Linux".
    std::optional<std::string> platform;
    /// Version of the OS.
    std::optional<std::string> platform_version;
    /// Any other details, as a JSON object. Null to leave them out.
    nlohmann::json platform_details;
};

/// Information about this library and the platform it runs on.
AboutClientData default_about_client();

/// An app-defined message between the client and the bot.
struct ClientMessageData {
    /// Type of the message, defined by your app.
    std::string type;
    /// Data of the message. Can be null.
    nlohmann::json data;
};

/// Options for sending text to the bot.
struct SendTextOptions {
    /// Whether the bot responds right away. Defaults to true.
    std::optional<bool> run_immediately;
    /// Whether the bot responds with audio. Defaults to true.
    std::optional<bool> audio_response;
};

/// The result of a function call, sent to the bot.
struct LLMFunctionCallResultData {
    /// Name of the function.
    std::string function_name;
    /// ID of the function call.
    std::string tool_call_id;
    /// Arguments the function was called with.
    nlohmann::json arguments;
    /// The result.
    nlohmann::json result;
};

/// Sent by the bot when it's ready.
struct BotReadyData {
    /// RTVI version of the bot.
    std::string version;
    /// Information about the bot. Can be null.
    nlohmann::json about;
};

/// An error.
struct ErrorData {
    /// What went wrong.
    std::string error;
    /// Whether the error is fatal, e.g. the bot stopped.
    bool fatal = false;
};

/// What the user said.
struct TranscriptData {
    /// The words.
    std::string text;
    /// Whether the text is final. If not, a new transcript will replace it.
    bool final = false;
    /// When it was said, as an ISO 8601 date.
    std::string timestamp;
    /// ID of the user who spoke.
    std::string user_id;
};

/// Some text.
struct TextData {
    /// The text.
    std::string text;
};

/// User text sent to the bot's LLM.
using UserLLMTextData = TextData;

/// Text from the bot's LLM, as it streams.
using BotLLMTextData = TextData;

/// Text the bot is speaking.
using BotTTSTextData = TextData;

/// How much of a text the bot has spoken.
struct SpokenProgressData {
    /// Text spoken so far, including the current word.
    std::string accumulated_text;
    /// Text not spoken yet.
    std::string remaining_text;
};

/// Something the bot says or outputs, e.g. a sentence.
///
/// If the bot speaks the text, it's sent again as the bot speaks it, with the
/// same `segment_id` and updated `spoken_status` and `spoken_progress`.
struct BotOutputData {
    /// The text.
    std::string text;
    /// What the text is, e.g. "word" or "sentence".
    std::string aggregated_by;
    /// ID shared by updates of a text.
    std::optional<int64_t> segment_id;
    /// Whether the bot will speak it.
    std::optional<bool> will_be_spoken;
    /// Where the bot is in speaking it: "new", "in-progress" or "completed".
    std::optional<std::string> spoken_status;
    /// How much of the text the bot has spoken. Only for text it speaks.
    std::optional<SpokenProgressData> spoken_progress;
};

/// A single measurement.
struct MetricData {
    /// What was measured, e.g. a service in the bot.
    std::string processor;
    /// The AI model used, if any.
    std::optional<std::string> model;
    /// The measured value, e.g. a time in seconds.
    double value = 0;
};

/// Performance metrics from the bot.
struct MetricsData {
    /// Times to first byte, in seconds.
    std::vector<MetricData> ttfb;
    /// Processing times, in seconds.
    std::vector<MetricData> processing;
    /// Characters sent to be spoken.
    std::vector<MetricData> characters;
    /// All the metrics, including those without a field above.
    nlohmann::json raw;
};

/// The bot's LLM started a function call.
struct LLMFunctionCallStartedData {
    /// Name of the function, if the bot shares it.
    std::optional<std::string> function_name;
};

/// A function call is running.
struct LLMFunctionCallInProgressData {
    /// Name of the function, if the bot shares it.
    std::optional<std::string> function_name;
    /// ID of the function call.
    std::string tool_call_id;
    /// Arguments of the call, as a JSON object. Empty if the bot doesn't
    /// share them.
    nlohmann::json arguments;
};

/// A function call finished or was cancelled.
struct LLMFunctionCallStoppedData {
    /// Name of the function, if the bot shares it.
    std::optional<std::string> function_name;
    /// ID of the function call.
    std::string tool_call_id;
    /// Whether the call was cancelled.
    bool cancelled = false;
    /// The result of the function, if the bot shares it. Can be null.
    nlohmann::json result;
};

/// A search result.
struct LLMSearchResult {
    /// Text of the result.
    std::string text;
    /// Confidence scores, from 0 to 1.
    std::vector<double> confidence;
};

/// Where search results come from, e.g. a website.
struct LLMSearchOrigin {
    /// Address of the site.
    std::optional<std::string> site_uri;
    /// Title of the site.
    std::optional<std::string> site_title;
    /// Results from this site.
    std::vector<LLMSearchResult> results;
};

/// Search results used by the bot's LLM.
struct BotLLMSearchResponseData {
    /// Summary of the results.
    std::optional<std::string> search_result;
    /// Results ready to show.
    std::optional<std::string> rendered_content;
    /// Where the results come from.
    std::vector<LLMSearchOrigin> origins;
};

/// An RTVI message.
///
/// PipecatClient creates and sends messages for you. You only need these to
/// write a transport, or to read messages the client doesn't handle.
struct Message {
    /// ID of the message. Answers have the request's ID.
    std::string id;
    /// Always "rtvi-ai".
    std::string label = MESSAGE_LABEL;
    /// Type of the message, e.g. "bot-ready".
    std::string type;
    /// Data of the message. Null if it has none.
    nlohmann::json data;

    /// An empty message.
    Message() = default;

    /// A message of `type` with `data`, and a new random ID.
    explicit Message(MessageType type, nlohmann::json data = nullptr);

    /// A message that says the client is ready, with information about it.
    static Message client_ready(const AboutClientData& about);

    /// A message that asks the bot to leave.
    static Message disconnect_bot();

    /// An app-defined message to the bot.
    static Message
    client_message(const std::string& type, const nlohmann::json& data);

    /// Text for the bot's LLM.
    static Message
    send_text(const std::string& content, const SendTextOptions& options = {});

    /// One or more DTMF keys (0-9, * and #), for bots on RTVI 2.1 or newer.
    static Message dtmf(const std::string& buttons);

    /// The result of a function call.
    static Message llm_function_call_result(
            const LLMFunctionCallResultData& data
    );
};

//
// JSON conversions, used by nlohmann::json (e.g. `data.get<BotOutputData>()`).
//

/// @cond

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

/// @endcond

}  // namespace pipecat::rtvi

#endif
