//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "helpers.h"

#include "pipecat/client.h"
#include "pipecat/errors.h"

#include <gtest/gtest.h>

#include <future>
#include <optional>
#include <thread>

using namespace pipecat;
using nlohmann::json;
using Events = std::vector<std::string>;

//
// Lifecycle
//

TEST(PipecatClient, RequiresTransport) {
    EXPECT_THROW(PipecatClient(PipecatClientOptions {}), PipecatError);
}

TEST(PipecatClient, InitializesOnce) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->initialize();
    client->initialize();
    EXPECT_EQ(transport->initialize_count, 1);
    EXPECT_EQ(client->state(), TransportState::Initialized);
}

TEST(PipecatClient, ConnectWaitsForBotReady) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);

    auto data = client->connect({{"room_url", "https://example.com/room"}});

    EXPECT_EQ(data.version, "2.1.0");
    EXPECT_EQ(client->state(), TransportState::Ready);
    EXPECT_TRUE(client->connected());
    EXPECT_EQ(
            transport->connect_params(),
            json({{"room_url", "https://example.com/room"}})
    );

    auto ready_message = transport->ready_message();
    ASSERT_TRUE(ready_message.has_value());
    EXPECT_EQ(ready_message->type, "client-ready");
    EXPECT_EQ(ready_message->data["version"], "2.1.0");
    EXPECT_EQ(ready_message->data["about"]["library"], "pipecat-client-cxx");

    ASSERT_TRUE(recorder.wait_for("bot-ready:2.1.0"));
    EXPECT_EQ(
            recorder.events(),
            (Events {
                    "state:initializing",
                    "state:initialized",
                    "state:connecting",
                    "state:connected",
                    "connected",
                    "state:ready",
                    "bot-ready:2.1.0",
            })
    );
}

TEST(PipecatClient, SendsAboutFromOptions) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.about.platform_details["app"] = "kiosk";
    });
    client->connect();
    EXPECT_EQ(
            transport->ready_message()
                    ->data["about"]["platform_details"]["app"],
            "kiosk"
    );
}

TEST(PipecatClient, ConnectTimesOut) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.connect_timeout = std::chrono::milliseconds(100);
    });
    transport->send_bot_ready = false;

    EXPECT_THROW(client->connect(), ConnectionTimeoutError);

    EXPECT_EQ(client->state(), TransportState::Error);
    EXPECT_FALSE(transport->connected());
    ASSERT_TRUE(recorder.wait_for("state:error"));
    EXPECT_EQ(
            recorder.events("state:"),
            (Events {
                    "state:initializing",
                    "state:initialized",
                    "state:connecting",
                    "state:connected",
                    "state:disconnecting",
                    "state:error",
            })
    );
    ASSERT_TRUE(recorder.wait_for("disconnected"));
}

TEST(PipecatClient, IgnoresBotReadyAfterTimeout) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.connect_timeout = std::chrono::milliseconds(100);
    });
    transport->send_bot_ready = false;
    EXPECT_THROW(client->connect(), ConnectionTimeoutError);

    transport->deliver_message(
            rtvi_message("bot-ready", {{"version", "2.1.0"}})
    );
    // Callbacks run in order, so any for bot-ready would come before this one.
    transport->deliver_participant_joined({"user-id", "User", false});
    ASSERT_TRUE(recorder.wait_for("participant-joined:user-id"));

    EXPECT_EQ(client->state(), TransportState::Error);
    EXPECT_TRUE(recorder.events("bot-ready").empty());
}

TEST(PipecatClient, TransportConnectFailure) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    transport->fail_connect = true;

    EXPECT_THROW(client->connect(), TransportStartError);

    EXPECT_EQ(client->state(), TransportState::Error);
    ASSERT_TRUE(recorder.wait_for("state:error"));
    EXPECT_TRUE(recorder.events("connected").empty());
}

TEST(PipecatClient, ConnectAgainAfterFailure) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    transport->fail_connect = true;
    EXPECT_THROW(client->connect(), TransportStartError);

    transport->fail_connect = false;
    client->connect();
    EXPECT_EQ(client->state(), TransportState::Ready);
}

TEST(PipecatClient, ConnectWhileConnectedThrows) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    EXPECT_THROW(client->connect(), BotAlreadyStartedError);
    APIRequest request;
    request.endpoint = "http://127.0.0.1:1/start";
    EXPECT_THROW(client->start_bot(request), BotAlreadyStartedError);
    EXPECT_EQ(client->state(), TransportState::Ready);
}

TEST(PipecatClient, Disconnect) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    client->disconnect();

    EXPECT_EQ(client->state(), TransportState::Disconnected);
    EXPECT_FALSE(client->connected());
    EXPECT_FALSE(transport->connected());
    ASSERT_TRUE(recorder.wait_for("disconnected"));
    EXPECT_EQ(
            recorder.events("state:"),
            (Events {
                    "state:initializing",
                    "state:initialized",
                    "state:connecting",
                    "state:connected",
                    "state:ready",
                    "state:disconnecting",
                    "state:disconnected",
            })
    );

    // Nothing left to do.
    client->disconnect();
    EXPECT_EQ(transport->disconnect_count, 1);
}

TEST(PipecatClient, Reconnects) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();
    client->disconnect();
    client->connect();
    EXPECT_EQ(client->state(), TransportState::Ready);
    EXPECT_EQ(transport->initialize_count, 1);
}

TEST(PipecatClient, DisconnectCancelsConnect) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.connect_timeout = std::chrono::milliseconds(0);
    });
    transport->send_bot_ready = false;

    std::thread disconnector([&, client = client.get()] {
        ASSERT_TRUE(recorder.wait_for("connected"));
        client->disconnect();
    });

    EXPECT_THROW(client->connect(), PipecatError);
    disconnector.join();

    EXPECT_EQ(client->state(), TransportState::Disconnected);
    EXPECT_FALSE(transport->connected());
}

TEST(PipecatClient, DisconnectWaitsForDisconnectionInProgress) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    // Hold the disconnection that starts when the bot leaves.
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    transport->on_disconnect = [&entered, released] {
        entered.set_value();
        released.wait();
    };
    transport->deliver_bot_disconnected();
    entered.get_future().wait();

    auto disconnected = std::async(std::launch::async, [client = client.get()] {
        client->disconnect();
    });
    EXPECT_EQ(
            disconnected.wait_for(std::chrono::milliseconds(100)),
            std::future_status::timeout
    );

    release.set_value();
    disconnected.get();
    ASSERT_TRUE(recorder.wait_for("disconnected"));
    transport->on_disconnect = nullptr;

    // Once disconnect() returns, the client can connect again.
    EXPECT_NO_THROW(client->connect());
}

TEST(PipecatClient, TransportDisconnects) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    transport->deliver_transport_disconnected();

    ASSERT_TRUE(recorder.wait_for("disconnected"));
    EXPECT_EQ(client->state(), TransportState::Disconnected);
}

TEST(PipecatClient, DestructorDisconnectsAndDeliversCallbacks) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    client.reset();

    // Delivered before the destructor returned, no waiting needed.
    auto events = recorder.events();
    EXPECT_EQ(events.back(), "disconnected");
}

TEST(PipecatClient, CallbacksCanUseTheClientWhileItsDestroyed) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::optional<TransportState> state;
    recorder.on_disconnected_hook = [&state, client = client.get()] {
        state = client->state();
    };
    client->connect();

    client.reset();

    EXPECT_EQ(state, TransportState::Disconnected);
}

TEST(PipecatClient, Audio) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    int16_t frames[160] = {};

    EXPECT_EQ(client->send_user_audio(frames, 160), 0);
    EXPECT_EQ(client->read_bot_audio(frames, 160), 0);

    client->connect();
    EXPECT_EQ(client->send_user_audio(frames, 160), 160);
    EXPECT_EQ(client->read_bot_audio(frames, 160), 160);

    client->disconnect();
    EXPECT_EQ(client->send_user_audio(frames, 160), 0);
    EXPECT_EQ(client->read_bot_audio(frames, 160), 0);
}

//
// Threading
//

TEST(PipecatClient, CallbacksRunOnOneThread) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();
    client->disconnect();
    ASSERT_TRUE(recorder.wait_for("disconnected"));

    auto threads = recorder.threads();
    for (const auto& thread: threads) {
        EXPECT_EQ(thread, threads.front());
    }
    EXPECT_NE(threads.front(), std::this_thread::get_id());
}

TEST(PipecatClient, DisconnectFromCallback) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.disconnect_on_bot_disconnect = false;
    });
    recorder.on_bot_disconnected_hook = [client = client.get()] {
        client->disconnect();
    };
    client->connect();

    transport->deliver_bot_disconnected();

    ASSERT_TRUE(recorder.wait_for("disconnected"));
    EXPECT_EQ(client->state(), TransportState::Disconnected);
    EXPECT_FALSE(transport->connected());
    EXPECT_FALSE(transport->deadlock_detected);
}

TEST(PipecatClient, DisconnectsWhenBotLeaves) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    transport->deliver_bot_disconnected();

    ASSERT_TRUE(recorder.wait_for("disconnected"));
    EXPECT_EQ(client->state(), TransportState::Disconnected);
    EXPECT_FALSE(transport->deadlock_detected);
    // on_bot_disconnected() runs before the disconnection.
    auto events = recorder.events();
    auto bot_left =
            std::find(events.begin(), events.end(), "bot-disconnected:bot-id");
    auto disconnected = std::find(events.begin(), events.end(), "disconnected");
    EXPECT_LT(bot_left, disconnected);
}

TEST(PipecatClient, StaysConnectedWhenBotLeaves) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder, [](auto& options) {
        options.disconnect_on_bot_disconnect = false;
    });
    client->connect();

    transport->deliver_bot_disconnected();

    ASSERT_TRUE(recorder.wait_for("bot-disconnected:bot-id"));
    EXPECT_EQ(client->state(), TransportState::Ready);
    EXPECT_TRUE(transport->connected());
}

TEST(PipecatClient, SlowCallbacksDontBlockTransport) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    std::promise<void> release;
    auto released = release.get_future().share();
    recorder.on_user_started_speaking_hook = [released] { released.wait(); };
    client->connect();

    transport->deliver_message(rtvi_message("user-started-speaking"));
    transport->deliver_message(rtvi_message("user-stopped-speaking"));
    // The transport's event thread handles both messages while the first
    // callback is still blocked.
    transport->flush();
    EXPECT_TRUE(recorder.events("user-stopped-speaking").empty());

    release.set_value();
    ASSERT_TRUE(recorder.wait_for("user-stopped-speaking"));
}

//
// Messages
//

TEST(PipecatClient, DispatchesMessages) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    const json MESSAGES[] = {
            rtvi_message("user-started-speaking"),
            rtvi_message(
                    "user-transcription", {{"text", "Hi"}, {"final", true}}
            ),
            rtvi_message("user-stopped-speaking"),
            rtvi_message("user-llm-text", {{"text", "Hi"}}),
            rtvi_message("user-mute-started"),
            rtvi_message("user-mute-stopped"),
            rtvi_message("bot-llm-started"),
            rtvi_message("bot-llm-text", {{"text", "Hel"}}),
            rtvi_message("bot-llm-stopped"),
            rtvi_message("bot-started-speaking"),
            rtvi_message("bot-tts-started"),
            rtvi_message("bot-tts-text", {{"text", "Hello"}}),
            rtvi_message(
                    "bot-output",
                    {{"text", "Hello"}, {"aggregated_by", "sentence"}}
            ),
            rtvi_message("bot-tts-stopped"),
            rtvi_message("bot-stopped-speaking"),
            rtvi_message(
                    "metrics",
                    {{"ttfb", {{{"processor", "llm"}, {"value", 0.2}}}}}
            ),
            rtvi_message("server-message", {{"score", 10}}),
            rtvi_message("error", {{"error", "Boom"}, {"fatal", true}}),
            rtvi_message("error-response", {{"error", "Unsupported"}}),
            rtvi_message(
                    "llm-function-call-started",
                    {{"function_name", "get_weather"}}
            ),
            rtvi_message(
                    "llm-function-call-in-progress",
                    {{"function_name", "get_weather"},
                     {"tool_call_id", "call_1"},
                     {"arguments", {{"city", "SF"}}}}
            ),
            rtvi_message(
                    "llm-function-call-stopped",
                    {{"tool_call_id", "call_1"}, {"cancelled", false}}
            ),
            rtvi_message(
                    "llm-function-call",
                    {{"function_name", "get_time"},
                     {"tool_call_id", "call_2"},
                     {"args", {{"zone", "UTC"}}}}
            ),
            rtvi_message(
                    "bot-llm-search-response",
                    {{"search_result", "Sunny"}, {"origins", json::array()}}
            ),
            rtvi_message("bot-transcription", {{"text", "Ignored"}}),
            rtvi_message("ui-command", {{"command", "click"}}),
            {{"label", "other"}, {"type", "bot-output"}},
            rtvi_message("server-message", {{"done", true}}),
    };
    for (const auto& message: MESSAGES) {
        transport->deliver_message(message);
    }

    ASSERT_TRUE(recorder.wait_for(R"(server-message:{"done":true})"));
    auto events = recorder.events();
    auto first =
            std::find(events.begin(), events.end(), "user-started-speaking");
    EXPECT_EQ(
            Events(first, events.end()),
            (Events {
                    "user-started-speaking",
                    "user-transcript:Hi:final",
                    "user-stopped-speaking",
                    "user-llm-text:Hi",
                    "user-mute-started",
                    "user-mute-stopped",
                    "bot-llm-started",
                    "bot-llm-text:Hel",
                    "bot-llm-stopped",
                    "bot-started-speaking",
                    "bot-tts-started",
                    "bot-tts-text:Hello",
                    "bot-output:Hello:sentence",
                    "bot-tts-stopped",
                    "bot-stopped-speaking",
                    "metrics:1",
                    R"(server-message:{"score":10})",
                    "error:Boom:fatal",
                    "message-error:Unsupported",
                    "function-call-started:get_weather",
                    R"(function-call-in-progress:get_weather:{"city":"SF"})",
                    "function-call-stopped:call_1",
                    R"(function-call-in-progress:get_time:{"zone":"UTC"})",
                    "bot-llm-search-response:Sunny",
                    "unhandled:ui-command",
                    R"(server-message:{"done":true})",
            })
    );
}

TEST(PipecatClient, MalformedMessageReportsError) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    transport->deliver_message(rtvi_message("bot-output", {{"text", 5}}));
    transport->deliver_message(rtvi_message("bot-output", {{"text", "Hi"}}));

    ASSERT_TRUE(recorder.wait_for("bot-output:Hi:"));
    auto errors = recorder.events("error:");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].rfind("error:Invalid RTVI message (bot-output)", 0), 0u)
            << errors[0];
}

TEST(PipecatClient, Participants) {
    Recorder recorder;
    auto [transport, client] = make_client(recorder);
    client->connect();

    Participant user {"user-id", "User", false};
    transport->deliver_bot_connected();
    transport->deliver_participant_joined(user);
    transport->deliver_participant_left(user);

    ASSERT_TRUE(recorder.wait_for("participant-left:user-id"));
    EXPECT_EQ(
            recorder.events("bot-connected"), (Events {"bot-connected:bot-id"})
    );
    EXPECT_EQ(
            recorder.events("participant-"),
            (Events {"participant-joined:user-id", "participant-left:user-id"})
    );
}

TEST(TransportState, Names) {
    EXPECT_STREQ(to_string(TransportState::Disconnected), "disconnected");
    EXPECT_STREQ(to_string(TransportState::Initializing), "initializing");
    EXPECT_STREQ(to_string(TransportState::Initialized), "initialized");
    EXPECT_STREQ(to_string(TransportState::Authenticating), "authenticating");
    EXPECT_STREQ(to_string(TransportState::Authenticated), "authenticated");
    EXPECT_STREQ(to_string(TransportState::Connecting), "connecting");
    EXPECT_STREQ(to_string(TransportState::Connected), "connected");
    EXPECT_STREQ(to_string(TransportState::Ready), "ready");
    EXPECT_STREQ(to_string(TransportState::Disconnecting), "disconnecting");
    EXPECT_STREQ(to_string(TransportState::Error), "error");
}
