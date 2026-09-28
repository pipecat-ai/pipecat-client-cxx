//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/rtvi/messages.h"
#include "pipecat/version.h"

#include <gtest/gtest.h>

#include <cctype>
#include <utility>

using namespace pipecat::rtvi;
using nlohmann::json;

//
// Message types
//

TEST(MessageType, RoundTripsAllTypes) {
    const std::pair<MessageType, const char*> TYPES[] = {
            {MessageType::ClientReady, "client-ready"},
            {MessageType::DisconnectBot, "disconnect-bot"},
            {MessageType::ClientMessage, "client-message"},
            {MessageType::SendText, "send-text"},
            {MessageType::DTMF, "dtmf"},
            {MessageType::LLMFunctionCallResult, "llm-function-call-result"},
            {MessageType::BotReady, "bot-ready"},
            {MessageType::Error, "error"},
            {MessageType::ErrorResponse, "error-response"},
            {MessageType::ServerMessage, "server-message"},
            {MessageType::ServerResponse, "server-response"},
            {MessageType::Metrics, "metrics"},
            {MessageType::UserStartedSpeaking, "user-started-speaking"},
            {MessageType::UserStoppedSpeaking, "user-stopped-speaking"},
            {MessageType::BotStartedSpeaking, "bot-started-speaking"},
            {MessageType::BotStoppedSpeaking, "bot-stopped-speaking"},
            {MessageType::UserMuteStarted, "user-mute-started"},
            {MessageType::UserMuteStopped, "user-mute-stopped"},
            {MessageType::UserTranscription, "user-transcription"},
            {MessageType::UserLLMText, "user-llm-text"},
            {MessageType::BotOutput, "bot-output"},
            {MessageType::BotLLMText, "bot-llm-text"},
            {MessageType::BotLLMStarted, "bot-llm-started"},
            {MessageType::BotLLMStopped, "bot-llm-stopped"},
            {MessageType::BotLLMSearchResponse, "bot-llm-search-response"},
            {MessageType::BotTTSText, "bot-tts-text"},
            {MessageType::BotTTSStarted, "bot-tts-started"},
            {MessageType::BotTTSStopped, "bot-tts-stopped"},
            {MessageType::LLMFunctionCallStarted, "llm-function-call-started"},
            {MessageType::LLMFunctionCallInProgress,
             "llm-function-call-in-progress"},
            {MessageType::LLMFunctionCallStopped, "llm-function-call-stopped"},
    };

    for (const auto& [type, name]: TYPES) {
        EXPECT_STREQ(to_string(type), name);
        EXPECT_EQ(parse_message_type(name), type) << name;
    }
}

TEST(MessageType, UnknownTypeIsNullopt) {
    EXPECT_EQ(parse_message_type("ui-command"), std::nullopt);
    EXPECT_EQ(parse_message_type(""), std::nullopt);
}

//
// Messages
//

TEST(Message, HasEnvelope) {
    Message message(MessageType::DisconnectBot);
    EXPECT_EQ(message.id.size(), 10u);
    for (char c: message.id) {
        EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(c)));
    }
    EXPECT_EQ(message.label, "rtvi-ai");
    EXPECT_EQ(message.type, "disconnect-bot");
    EXPECT_TRUE(message.data.is_null());
}

TEST(Message, HasUniqueIds) {
    EXPECT_NE(
            Message(MessageType::SendText).id, Message(MessageType::SendText).id
    );
}

TEST(Message, SerializesWithoutNullData) {
    Message message(MessageType::DisconnectBot);
    message.id = "1";
    EXPECT_EQ(
            json(message),
            json::parse(
                    R"({"id": "1", "label": "rtvi-ai", "type": "disconnect-bot"})"
            )
    );
    message.data = {{"a", 1}};
    EXPECT_EQ(json(message)["data"], json({{"a", 1}}));
}

TEST(Message, ParsesServerMessage) {
    // Most server messages have no id.
    auto message = json::parse(R"({
        "label": "rtvi-ai", "type": "bot-output", "data": {"text": "Hi"}
    })")
                           .get<Message>();
    EXPECT_EQ(message.id, "");
    EXPECT_EQ(message.label, "rtvi-ai");
    EXPECT_EQ(message.type, "bot-output");
    EXPECT_EQ(message.data["text"], "Hi");

    auto empty = json::parse(R"({"type": "bot-ready"})").get<Message>();
    EXPECT_EQ(empty.label, "");
    EXPECT_TRUE(empty.data.is_null());
}

TEST(Message, ClientReady) {
    AboutClientData about;
    about.library = "test";
    auto message = Message::client_ready(about);
    EXPECT_EQ(message.type, "client-ready");
    EXPECT_EQ(
            message.data,
            json::parse(R"({"version": "2.1.0", "about": {"library": "test"}})")
    );

    about.library_version = "1.0.0";
    about.platform = "Linux";
    about.platform_version = "6.0";
    about.platform_details = {{"arch", "x86_64"}};
    EXPECT_EQ(Message::client_ready(about).data["about"], json::parse(R"({
        "library": "test",
        "library_version": "1.0.0",
        "platform": "Linux",
        "platform_version": "6.0",
        "platform_details": {"arch": "x86_64"}
    })"));
}

TEST(Message, DisconnectBot) {
    auto message = Message::disconnect_bot();
    EXPECT_EQ(message.type, "disconnect-bot");
    EXPECT_EQ(message.data, json::object());
}

TEST(Message, ClientMessage) {
    auto message = Message::client_message("get-weather", {{"city", "SF"}});
    EXPECT_EQ(message.type, "client-message");
    EXPECT_EQ(
            message.data,
            json::parse(R"({"t": "get-weather", "d": {"city": "SF"}})")
    );
    EXPECT_EQ(
            Message::client_message("ping", nullptr).data,
            json::parse(R"({"t": "ping"})")
    );
}

TEST(Message, SendText) {
    auto message = Message::send_text("Hello");
    EXPECT_EQ(message.type, "send-text");
    EXPECT_EQ(
            message.data, json::parse(R"({"content": "Hello", "options": {}})")
    );

    SendTextOptions options;
    options.run_immediately = false;
    options.audio_response = true;
    EXPECT_EQ(
            Message::send_text("Hello", options).data["options"],
            json::parse(R"({
        "run_immediately": false, "audio_response": true
    })")
    );
}

TEST(Message, DTMF) {
    auto message = Message::dtmf("12#");
    EXPECT_EQ(message.type, "dtmf");
    EXPECT_EQ(message.data, json::parse(R"({"buttons": ["1", "2", "#"]})"));
}

TEST(Message, LLMFunctionCallResult) {
    LLMFunctionCallResultData data;
    data.function_name = "get_weather";
    data.tool_call_id = "call_1";
    data.result = {{"temperature", 20}};
    auto message = Message::llm_function_call_result(data);
    EXPECT_EQ(message.type, "llm-function-call-result");
    EXPECT_EQ(message.data, json::parse(R"({
        "function_name": "get_weather",
        "tool_call_id": "call_1",
        "arguments": {},
        "result": {"temperature": 20}
    })"));
}

TEST(AboutClientData, DescribesThisLibrary) {
    auto about = default_about_client();
    EXPECT_EQ(about.library, "pipecat-client-cxx");
    EXPECT_EQ(about.library_version, PIPECAT_VERSION);
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    EXPECT_TRUE(about.platform.has_value());
#endif
    EXPECT_TRUE(about.platform_details.contains("arch"));
    EXPECT_TRUE(about.platform_details.contains("compiler"));
}

//
// Server to client data
//

TEST(ServerData, BotReady) {
    auto data = json::parse(R"({"version": "2.1.0", "about": null})")
                        .get<BotReadyData>();
    EXPECT_EQ(data.version, "2.1.0");
    EXPECT_TRUE(data.about.is_null());

    data = json::parse(
                   R"({"version": "2.1.0", "about": {"library": "pipecat"}})"
    )
                   .get<BotReadyData>();
    EXPECT_EQ(data.about["library"], "pipecat");
}

TEST(ServerData, Error) {
    auto data =
            json::parse(R"({"error": "Boom", "fatal": true})").get<ErrorData>();
    EXPECT_EQ(data.error, "Boom");
    EXPECT_TRUE(data.fatal);

    // `error-response` has no `fatal`.
    data = json::parse(R"({"error": "Unsupported type"})").get<ErrorData>();
    EXPECT_EQ(data.error, "Unsupported type");
    EXPECT_FALSE(data.fatal);
}

TEST(ServerData, Transcript) {
    auto data = json::parse(R"({
        "text": "Hello", "final": true, "timestamp": "2026-09-28T10:00:00",
        "user_id": "user"
    })")
                        .get<TranscriptData>();
    EXPECT_EQ(data.text, "Hello");
    EXPECT_TRUE(data.final);
    EXPECT_EQ(data.timestamp, "2026-09-28T10:00:00");
    EXPECT_EQ(data.user_id, "user");
}

TEST(ServerData, Text) {
    EXPECT_EQ(
            json::parse(R"({"text": "Hi"})").get<BotLLMTextData>().text, "Hi"
    );
    EXPECT_EQ(json::parse("{}").get<BotTTSTextData>().text, "");
    EXPECT_EQ(json(nullptr).get<TextData>().text, "");
}

TEST(ServerData, BotOutput) {
    auto data = json::parse(R"({
        "text": "Hello there.",
        "aggregated_by": "sentence",
        "segment_id": 3,
        "will_be_spoken": true,
        "spoken_status": "in-progress",
        "spoken_progress": {
            "accumulated_text": "Hello",
            "remaining_text": " there."
        }
    })")
                        .get<BotOutputData>();
    EXPECT_EQ(data.text, "Hello there.");
    EXPECT_EQ(data.aggregated_by, "sentence");
    EXPECT_EQ(data.segment_id, 3);
    EXPECT_EQ(data.will_be_spoken, true);
    EXPECT_EQ(data.spoken_status, "in-progress");
    ASSERT_TRUE(data.spoken_progress.has_value());
    EXPECT_EQ(data.spoken_progress->accumulated_text, "Hello");
    EXPECT_EQ(data.spoken_progress->remaining_text, " there.");
}

TEST(ServerData, BotOutputMinimal) {
    auto data = json::parse(R"({
        "text": "Hi", "aggregated_by": "word", "segment_id": null
    })")
                        .get<BotOutputData>();
    EXPECT_EQ(data.text, "Hi");
    EXPECT_EQ(data.segment_id, std::nullopt);
    EXPECT_EQ(data.will_be_spoken, std::nullopt);
    EXPECT_EQ(data.spoken_status, std::nullopt);
    EXPECT_FALSE(data.spoken_progress.has_value());
}

TEST(ServerData, Metrics) {
    auto raw = json::parse(R"({
        "ttfb": [{"processor": "OpenAILLMService#0", "model": "gpt-4o", "value": 0.25}],
        "processing": [{"processor": "CartesiaTTSService#0", "value": 1}],
        "tokens": [{"prompt_tokens": 10, "completion_tokens": 5, "total_tokens": 15}]
    })");
    auto data = raw.get<MetricsData>();
    ASSERT_EQ(data.ttfb.size(), 1u);
    EXPECT_EQ(data.ttfb[0].processor, "OpenAILLMService#0");
    EXPECT_EQ(data.ttfb[0].model, "gpt-4o");
    EXPECT_DOUBLE_EQ(data.ttfb[0].value, 0.25);
    ASSERT_EQ(data.processing.size(), 1u);
    EXPECT_EQ(data.processing[0].model, std::nullopt);
    EXPECT_DOUBLE_EQ(data.processing[0].value, 1.0);
    EXPECT_TRUE(data.characters.empty());
    EXPECT_EQ(data.raw, raw);
}

TEST(ServerData, LLMFunctionCallStarted) {
    auto data = json::parse(R"({"function_name": "get_weather"})")
                        .get<LLMFunctionCallStartedData>();
    EXPECT_EQ(data.function_name, "get_weather");
    EXPECT_EQ(
            json::parse("{}").get<LLMFunctionCallStartedData>().function_name,
            std::nullopt
    );
}

TEST(ServerData, LLMFunctionCallInProgress) {
    auto data = json::parse(R"({
        "function_name": "get_weather",
        "tool_call_id": "call_1",
        "arguments": {"city": "SF"}
    })")
                        .get<LLMFunctionCallInProgressData>();
    EXPECT_EQ(data.function_name, "get_weather");
    EXPECT_EQ(data.tool_call_id, "call_1");
    EXPECT_EQ(data.arguments, json({{"city", "SF"}}));

    // Arguments not reported.
    data = json::parse(R"({"tool_call_id": "call_1", "arguments": null})")
                   .get<LLMFunctionCallInProgressData>();
    EXPECT_EQ(data.function_name, std::nullopt);
    EXPECT_EQ(data.arguments, json::object());
}

TEST(ServerData, LLMFunctionCallStopped) {
    auto data = json::parse(R"({
        "function_name": "get_weather", "tool_call_id": "call_1",
        "cancelled": false, "result": {"temperature": 20}
    })")
                        .get<LLMFunctionCallStoppedData>();
    EXPECT_EQ(data.tool_call_id, "call_1");
    EXPECT_FALSE(data.cancelled);
    EXPECT_EQ(data.result, json({{"temperature", 20}}));

    data = json::parse(R"({"tool_call_id": "call_1", "cancelled": true})")
                   .get<LLMFunctionCallStoppedData>();
    EXPECT_TRUE(data.cancelled);
    EXPECT_TRUE(data.result.is_null());
}

TEST(ServerData, BotLLMSearchResponse) {
    auto data = json::parse(R"({
        "search_result": "Sunny",
        "rendered_content": null,
        "origins": [{
            "site_uri": "https://example.com",
            "site_title": "Example",
            "results": [{"text": "Sunny today", "confidence": [0.9]}]
        }]
    })")
                        .get<BotLLMSearchResponseData>();
    EXPECT_EQ(data.search_result, "Sunny");
    EXPECT_EQ(data.rendered_content, std::nullopt);
    ASSERT_EQ(data.origins.size(), 1u);
    EXPECT_EQ(data.origins[0].site_title, "Example");
    ASSERT_EQ(data.origins[0].results.size(), 1u);
    EXPECT_EQ(data.origins[0].results[0].text, "Sunny today");
    EXPECT_EQ(data.origins[0].results[0].confidence, std::vector<double> {0.9});
}

TEST(ServerData, ServerResponse) {
    auto data = json::parse(R"({"t": "get-weather", "d": {"temperature": 20}})")
                        .get<ClientMessageData>();
    EXPECT_EQ(data.type, "get-weather");
    EXPECT_EQ(data.data, json({{"temperature", 20}}));
}

TEST(ServerData, WrongTypeThrows) {
    EXPECT_THROW(
            json::parse(R"({"text": 5})").get<TextData>(), json::exception
    );
}
