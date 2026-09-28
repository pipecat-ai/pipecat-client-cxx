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

TEST(DTMF, SendsOneKeyPerMessageToProtocol2_0Bots) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    transport->bot_version = "2.0.0";
    client->connect();

    client->send_dtmf("1*");

    auto sent = transport->sent_messages("dtmf");
    ASSERT_EQ(sent.size(), 2u);
    EXPECT_EQ(sent[0].data, json::parse(R"({"button": "1"})"));
    EXPECT_EQ(sent[1].data, json::parse(R"({"button": "*"})"));
}

TEST(DTMF, UnsupportedByOlderBots) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    transport->bot_version = "1.4.0";
    client->connect();

    EXPECT_THROW(client->send_dtmf("1"), UnsupportedFeatureError);
    EXPECT_TRUE(transport->sent_messages("dtmf").empty());
}

TEST(DTMF, RejectsInvalidKeys) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    EXPECT_THROW(client->send_dtmf("12a"), PipecatError);
    EXPECT_THROW(client->send_dtmf(""), PipecatError);
    EXPECT_TRUE(transport->sent_messages("dtmf").empty());
}
