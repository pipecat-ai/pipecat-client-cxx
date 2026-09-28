//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "helpers.h"

#include "pipecat/client.h"
#include "pipecat/errors.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

using namespace pipecat;
using nlohmann::json;

namespace {

// Answers the last client-message with a server-response.
void answer_request(FakeTransport* transport, const json& data) {
    auto request = transport->sent_messages("client-message").back();
    transport->deliver_message(rtvi_message(
            "server-response",
            {{"t", request.data["t"]}, {"d", data}},
            request.id
    ));
}

template<typename T>
bool is_ready(const std::future<T>& future) {
    return future.wait_for(std::chrono::seconds(5)) ==
           std::future_status::ready;
}

}  // namespace

//
// Sending
//

TEST(Messaging, RequiresReadyBot) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);

    EXPECT_THROW(client->send_text("Hi"), BotNotReadyError);
    EXPECT_THROW(client->send_client_message("ping"), BotNotReadyError);
    EXPECT_THROW(client->send_client_request("ping"), BotNotReadyError);
    EXPECT_THROW(
            client->send_client_request("ping", nullptr, [](const auto&) {}),
            BotNotReadyError
    );
    EXPECT_THROW(client->disconnect_bot(), BotNotReadyError);
    EXPECT_THROW(client->send_dtmf("1"), BotNotReadyError);
    EXPECT_TRUE(transport->sent_messages().empty());
}

TEST(Messaging, SendText) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    rtvi::SendTextOptions options;
    options.run_immediately = false;
    client->send_text("Hello", options);

    auto sent = transport->sent_messages("send-text");
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0].data, json::parse(R"({
        "content": "Hello", "options": {"run_immediately": false}
    })"));
}

TEST(Messaging, SendClientMessage) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    client->send_client_message("score", {{"points", 10}});

    auto sent = transport->sent_messages("client-message");
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(
            sent[0].data, json::parse(R"({"t": "score", "d": {"points": 10}})")
    );
}

TEST(Messaging, DisconnectBot) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    client->disconnect_bot();

    EXPECT_EQ(transport->sent_messages("disconnect-bot").size(), 1u);
    EXPECT_EQ(client->state(), TransportState::Ready);
}

TEST(Messaging, RejectsLargeMessages) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();
    transport->max_size = 100;

    EXPECT_THROW(
            client->send_text(std::string(200, 'a')), MessageTooLargeError
    );
    EXPECT_TRUE(transport->sent_messages("send-text").empty());
}

//
// Client requests
//

TEST(ClientRequest, Future) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    auto future = client->send_client_request("get-weather", {{"city", "SF"}});
    auto request = transport->sent_messages("client-message").back();
    EXPECT_EQ(
            request.data,
            json::parse(R"({"t": "get-weather", "d": {"city": "SF"}})")
    );
    answer_request(transport, {{"temperature", 20}});

    ASSERT_TRUE(is_ready(future));
    EXPECT_EQ(future.get(), json({{"temperature", 20}}));
}

TEST(ClientRequest, ErrorResponse) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    auto future = client->send_client_request("get-weather");
    auto request = transport->sent_messages("client-message").back();
    transport->deliver_message(
            rtvi_message("error-response", {{"error", "Unknown"}}, request.id)
    );

    ASSERT_TRUE(is_ready(future));
    try {
        future.get();
        FAIL() << "get() didn't throw";
    } catch (const RequestTimeoutError&) {
        FAIL() << "Unexpected RequestTimeoutError";
    } catch (const MessageError& e) {
        EXPECT_STREQ(e.what(), "Unknown");
    }
    // Error responses are also reported to the callbacks.
    ASSERT_TRUE(recorder.wait_for("message-error:Unknown"));
}

TEST(ClientRequest, Timeout) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    auto future = client->send_client_request(
            "get-weather", nullptr, std::chrono::milliseconds(50)
    );

    ASSERT_TRUE(is_ready(future));
    EXPECT_THROW(future.get(), RequestTimeoutError);

    // A late answer is ignored.
    answer_request(transport, {{"temperature", 20}});
    transport->flush();
}

TEST(ClientRequest, Callback) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    std::promise<std::pair<ClientResponse, std::thread::id>> answered;
    client->send_client_request(
            "get-weather",
            nullptr,
            [&](const ClientResponse& response) {
                answered.set_value({response, std::this_thread::get_id()});
            }
    );
    answer_request(transport, {{"temperature", 20}});

    auto future = answered.get_future();
    ASSERT_TRUE(is_ready(future));
    auto [response, thread] = future.get();
    EXPECT_EQ(response.data, json({{"temperature", 20}}));
    EXPECT_FALSE(response.error.has_value());
    EXPECT_FALSE(response.timed_out);
    // Runs on the event thread, like the other callbacks.
    EXPECT_EQ(thread, recorder.threads().front());
}

TEST(ClientRequest, CallbackTimeout) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    std::promise<ClientResponse> answered;
    client->send_client_request(
            "get-weather",
            nullptr,
            [&](const ClientResponse& response) {
                answered.set_value(response);
            },
            std::chrono::milliseconds(50)
    );

    auto future = answered.get_future();
    ASSERT_TRUE(is_ready(future));
    auto response = future.get();
    EXPECT_TRUE(response.error.has_value());
    EXPECT_TRUE(response.timed_out);
    EXPECT_TRUE(response.data.is_null());
}

TEST(ClientRequest, DisconnectCancels) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    auto future = client->send_client_request(
            "get-weather", nullptr, std::chrono::milliseconds(0)
    );
    std::promise<ClientResponse> answered;
    client->send_client_request(
            "get-time",
            nullptr,
            [&](const ClientResponse& response) {
                answered.set_value(response);
            }
    );

    client->disconnect();

    ASSERT_TRUE(is_ready(future));
    try {
        future.get();
        FAIL() << "get() didn't throw";
    } catch (const RequestTimeoutError&) {
        FAIL() << "Unexpected RequestTimeoutError";
    } catch (const MessageError&) {
    }
    auto callback_future = answered.get_future();
    ASSERT_TRUE(is_ready(callback_future));
    auto response = callback_future.get();
    EXPECT_TRUE(response.error.has_value());
    EXPECT_FALSE(response.timed_out);
}

TEST(ClientRequest, FailureReportedOnce) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    // Disconnecting while the request is sent cancels it, then sending fails.
    transport->on_send_message = [client = client.get()] {
        client->disconnect();
        throw PipecatError("Unable to send");
    };
    std::future<json> future;
    EXPECT_NO_THROW(
            future = client->send_client_request("get-weather", nullptr)
    );
    transport->on_send_message = nullptr;

    // Only the request reports it.
    ASSERT_TRUE(is_ready(future));
    EXPECT_THROW(future.get(), MessageError);
}

TEST(ClientRequest, WaitInsideCallback) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::promise<json> answered;
    recorder.on_user_started_speaking_hook = [&, client = client.get()] {
        // Blocks the event thread until the bot answers.
        answered.set_value(client->send_client_request("ping").get());
    };
    client->connect();

    transport->deliver_message(rtvi_message("user-started-speaking"));
    ASSERT_TRUE(transport->wait_for_sent("client-message"));
    answer_request(transport, "pong");

    auto future = answered.get_future();
    ASSERT_TRUE(is_ready(future));
    EXPECT_EQ(future.get(), "pong");
}

//
// DTMF
//

TEST(DTMF, SendsAllKeysInOneMessage) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    client->send_dtmf("12#");

    auto sent = transport->sent_messages("dtmf");
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0].data, json::parse(R"({"buttons": ["1", "2", "#"]})"));
}

TEST(DTMF, UnsupportedByOlderBots) {
    for (const char* version: {"2.0.0", "1.4.0"}) {
        Recorder recorder;
        auto [transport, client] = make_client(recorder);
        transport->bot_version = version;
        client->connect();

        EXPECT_THROW(client->send_dtmf("1"), UnsupportedFeatureError)
                << version;
        EXPECT_TRUE(transport->sent_messages("dtmf").empty());
    }
}

TEST(DTMF, RejectsInvalidKeys) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    EXPECT_THROW(client->send_dtmf("12a"), PipecatError);
    EXPECT_THROW(client->send_dtmf(""), PipecatError);
    EXPECT_TRUE(transport->sent_messages("dtmf").empty());
}

//
// Function calls
//

namespace {

json function_call(
        const std::string& function_name,
        const std::string& tool_call_id
) {
    json data = {
            {"tool_call_id", tool_call_id},
            {"arguments", {{"city", "SF"}}},
    };
    if (!function_name.empty()) {
        data["function_name"] = function_name;
    }
    return rtvi_message("llm-function-call-in-progress", data);
}

// Waits until the event thread handled everything delivered so far.
void flush(FakeTransport* transport, Recorder& recorder) {
    static int marker = 0;
    std::string type = "flush-" + std::to_string(++marker);
    transport->deliver_message(rtvi_message(type));
    ASSERT_TRUE(recorder.wait_for("unhandled:" + type));
}

}  // namespace

TEST(FunctionCall, HandlerResponds) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::promise<std::pair<FunctionCallParams, std::thread::id>> called;
    client->register_function_call_handler(
            "get_weather",
            [&](const FunctionCallParams& params,
                FunctionCallResultCallback respond) {
                called.set_value({params, std::this_thread::get_id()});
                respond({{"temperature", 20}});
            }
    );
    client->connect();

    transport->deliver_message(function_call("get_weather", "call_1"));

    ASSERT_TRUE(transport->wait_for_sent("llm-function-call-result"));
    EXPECT_EQ(
            transport->sent_messages("llm-function-call-result")[0].data,
            json::parse(R"({
                "function_name": "get_weather",
                "tool_call_id": "call_1",
                "arguments": {"city": "SF"},
                "result": {"temperature": 20}
            })")
    );
    auto [params, thread] = called.get_future().get();
    EXPECT_EQ(params.function_name, "get_weather");
    EXPECT_EQ(params.arguments, json({{"city", "SF"}}));
    EXPECT_NE(thread, std::this_thread::get_id());
    // The in-progress callback still runs.
    ASSERT_TRUE(recorder.wait_for(
            R"(function-call-in-progress:get_weather:{"city":"SF"})"
    ));
}

TEST(FunctionCall, HandlerRespondsLater) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::promise<FunctionCallResultCallback> called;
    client->register_function_call_handler(
            "get_weather",
            [&](const FunctionCallParams&, FunctionCallResultCallback respond) {
                called.set_value(respond);
            }
    );
    client->connect();
    transport->deliver_message(function_call("get_weather", "call_1"));

    auto respond = called.get_future().get();
    std::thread([respond] { respond(nullptr); }).join();
    // Only the first answer counts.
    respond({{"ignored", true}});

    ASSERT_TRUE(transport->wait_for_sent("llm-function-call-result"));
    flush(transport, recorder);
    auto sent = transport->sent_messages("llm-function-call-result");
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0].data["result"], json::object());
}

TEST(FunctionCall, OnlyMatchingHandlerRuns) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    int calls = 0;
    client->register_function_call_handler(
            "get_weather",
            [&](const FunctionCallParams&, FunctionCallResultCallback) {
                calls++;
            }
    );
    client->connect();

    transport->deliver_message(function_call("get_time", "call_1"));
    // Without a name, it can't be matched to a handler.
    transport->deliver_message(function_call("", "call_2"));
    flush(transport, recorder);
    EXPECT_EQ(calls, 0);

    client->unregister_function_call_handler("get_weather");
    transport->deliver_message(function_call("get_weather", "call_3"));
    flush(transport, recorder);
    EXPECT_EQ(calls, 0);
}

TEST(FunctionCall, RespondAfterClientIsDestroyed) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::promise<FunctionCallResultCallback> called;
    client->register_function_call_handler(
            "get_weather",
            [&](const FunctionCallParams&, FunctionCallResultCallback respond) {
                called.set_value(respond);
            }
    );
    client->connect();
    transport->deliver_message(function_call("get_weather", "call_1"));
    auto respond = called.get_future().get();

    client.reset();

    // Ignored, and doesn't touch the destroyed client.
    respond({{"temperature", 20}});
}
