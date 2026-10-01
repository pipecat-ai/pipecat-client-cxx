//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "frames.h"

#include <pipecat/errors.h>
#include <pipecat/websocket/transport.h>

#include <gtest/gtest.h>
#include <rtc/rtc.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

using namespace pipecat;
using nlohmann::json;

namespace {

const std::chrono::seconds TIMEOUT {5};

// Records what the transport reports, from any thread.
class Observer : public TransportObserver {
   public:
    void on_transport_message(const json& message) override {
        add("message:" + message.value("type", ""));
    }
    void on_bot_connected(const Participant&) override {}
    void on_bot_disconnected(const Participant&) override {}
    void on_participant_joined(const Participant&) override {}
    void on_participant_left(const Participant&) override {}
    void on_transport_error(const std::string& error, bool fatal) override {
        add("error:" + error + (fatal ? ":fatal" : ""));
    }
    void on_transport_disconnected() override { add("disconnected"); }

    // Waits until an event that starts with `prefix` is reported.
    bool wait_for(const std::string& prefix) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _cv.wait_for(lock, TIMEOUT, [&] {
            for (const auto& event: _events) {
                if (event.rfind(prefix, 0) == 0) {
                    return true;
                }
            }
            return false;
        });
    }

    std::vector<std::string> events() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _events;
    }

   private:
    void add(const std::string& event) {
        std::lock_guard<std::mutex> lock(_mutex);
        _events.push_back(event);
        _cv.notify_all();
    }

    std::mutex _mutex;
    std::condition_variable _cv;
    std::vector<std::string> _events;
};

// A bot's WebSocket on localhost, on a free port. It keeps the frames the
// transport sends.
class FakeBot {
   public:
    FakeBot() : _server(server_config()) {
        _server.onClient([this](std::shared_ptr<rtc::WebSocket> ws) {
            ws->onOpen([this, ws] {
                std::lock_guard<std::mutex> lock(_mutex);
                _client = ws;
                _cv.notify_all();
            });
            ws->onClosed([this] {
                std::lock_guard<std::mutex> lock(_mutex);
                _closed = true;
                _cv.notify_all();
            });
            ws->onMessage(
                    [this](rtc::binary data) {
                        std::lock_guard<std::mutex> lock(_mutex);
                        _frames.push_back(
                                websocket::decode(data.data(), data.size())
                        );
                        _cv.notify_all();
                    },
                    [](rtc::string) {}
            );
        });
    }

    ~FakeBot() {
        // libdatachannel can't reset callbacks while it may still call them,
        // so wait for the last one.
        if (auto client = wait_for_client()) {
            client->close();
            wait_for_closed();
            client->resetCallbacks();
        }
        _server.stop();
    }

    std::string url() const {
        return "ws://127.0.0.1:" + std::to_string(_server.port());
    }

    std::shared_ptr<rtc::WebSocket> wait_for_client() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait_for(lock, TIMEOUT, [this] { return _client != nullptr; });
        return _client;
    }

    bool wait_for_closed() {
        std::unique_lock<std::mutex> lock(_mutex);
        return _cv.wait_for(lock, TIMEOUT, [this] { return _closed; });
    }

    std::optional<websocket::Frame> wait_for_frame() {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_cv.wait_for(lock, TIMEOUT, [this] { return !_frames.empty(); })) {
            return std::nullopt;
        }
        websocket::Frame frame = _frames.front();
        _frames.pop_front();
        return frame;
    }

    void send(const std::string& frame) {
        wait_for_client()->send(
                reinterpret_cast<const std::byte*>(frame.data()), frame.size()
        );
    }

   private:
    static rtc::WebSocketServer::Configuration server_config() {
        rtc::WebSocketServer::Configuration config;
        config.port = 0;
        config.bindAddress = "127.0.0.1";
        return config;
    }

    rtc::WebSocketServer _server;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::shared_ptr<rtc::WebSocket> _client;
    bool _closed = false;
    std::deque<websocket::Frame> _frames;
};

}  // namespace

TEST(WebSocketTransport, NeedsTheBotsUrl) {
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);

    EXPECT_THROW(
            transport.connect(json::object()), InvalidTransportParamsError
    );
    EXPECT_THROW(transport.connect(json::array()), InvalidTransportParamsError);
}

TEST(WebSocketTransport, NeedsAWebSocketUrl) {
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);

    EXPECT_THROW(
            transport.connect({{"wsUrl", "http://127.0.0.1:1"}}),
            InvalidTransportParamsError
    );
    EXPECT_TRUE(observer.events().empty());
}

TEST(WebSocketTransport, FailsWithoutABot) {
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);

    EXPECT_THROW(
            transport.connect({{"wsUrl", "ws://127.0.0.1:1"}}),
            TransportStartError
    );
}

TEST(WebSocketTransport, SendsTheTokenInTheUrl) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);

    transport.connect(
            {{"wsUrl", bot.url() + "/ws-client"}, {"token", "a b/c"}}
    );

    EXPECT_EQ(bot.wait_for_client()->path(), "/ws-client?token=a%20b%2Fc");
}

TEST(WebSocketTransport, ExchangesMessages) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    transport.send_message(rtvi::Message::client_message("ping", {{"n", 1}}));
    auto frame = bot.wait_for_frame();
    ASSERT_TRUE(frame);
    json message = json::parse(std::get<websocket::MessageFrame>(*frame).data);
    EXPECT_EQ(message["type"], "client-message");
    EXPECT_EQ(message["data"]["t"], "ping");

    bot.send(
            websocket::encode_message(
                    R"({"label":"rtvi-ai","type":"bot-ready","data":{}})"
            )
    );
    EXPECT_TRUE(observer.wait_for("message:bot-ready"));
}

TEST(WebSocketTransport, SendsUserAudio) {
    FakeBot bot;
    Observer observer;
    WebSocketTransportOptions options;
    options.user_audio_sample_rate = 24000;
    WebSocketTransport transport(options);
    transport.initialize(&observer);

    int16_t frames[160] = {};
    EXPECT_EQ(transport.send_user_audio(frames, 160), 0);

    transport.connect({{"wsUrl", bot.url()}});
    frames[0] = 1234;
    EXPECT_EQ(transport.send_user_audio(frames, 160), 160);

    auto frame = bot.wait_for_frame();
    ASSERT_TRUE(frame);
    auto audio = std::get<websocket::AudioFrame>(*frame);
    EXPECT_EQ(audio.samples.size(), 160u);
    EXPECT_EQ(audio.samples[0], 1234);
    EXPECT_EQ(audio.sample_rate, 24000u);
    EXPECT_EQ(audio.num_channels, 1u);
}

TEST(WebSocketTransport, ReceivesBotAudio) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);

    int16_t frames[160] = {};
    EXPECT_EQ(transport.read_bot_audio(frames, 160), 0);

    transport.connect({{"wsUrl", bot.url()}});
    std::vector<int16_t> samples(160, 1234);
    bot.send(websocket::encode_audio(samples.data(), 160, 16000, 1));

    EXPECT_EQ(transport.read_bot_audio(frames, 160), 160);
    EXPECT_EQ(std::vector<int16_t>(frames, frames + 160), samples);
}

TEST(WebSocketTransport, ConvertsBotAudio) {
    FakeBot bot;
    Observer observer;
    WebSocketTransportOptions options;
    options.bot_audio_channels = 2;
    WebSocketTransport transport(options);
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    // 20 ms at 24 kHz, mono.
    std::vector<int16_t> samples(480, 1234);
    bot.send(websocket::encode_audio(samples.data(), 480, 24000, 1));

    // 20 ms at 16 kHz, stereo.
    int16_t frames[320 * 2] = {};
    EXPECT_EQ(transport.read_bot_audio(frames, 320), 320);
    EXPECT_NEAR(frames[638], 1234, 20);
    for (size_t i = 0; i < 640; i += 2) {
        EXPECT_EQ(frames[i], frames[i + 1]);
    }
}

TEST(WebSocketTransport, DropsBotAudioWhenInterrupted) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    std::vector<int16_t> interrupted(160, 1);
    std::vector<int16_t> samples(160, 2);
    bot.send(websocket::encode_audio(interrupted.data(), 160, 16000, 1));
    // An InterruptionFrame, without fields.
    bot.send(std::string("\x2a\x00", 2));
    bot.send(websocket::encode_audio(samples.data(), 160, 16000, 1));
    // Frames arrive in order, so the audio is there after this message.
    bot.send(
            websocket::encode_message(
                    R"({"label":"rtvi-ai","type":"bot-ready","data":{}})"
            )
    );
    ASSERT_TRUE(observer.wait_for("message:bot-ready"));

    int16_t frames[160] = {};
    EXPECT_EQ(transport.read_bot_audio(frames, 160), 160);
    EXPECT_EQ(std::vector<int16_t>(frames, frames + 160), samples);
}

TEST(WebSocketTransport, ReportsWhenTheBotDisconnects) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    bot.wait_for_client()->close();

    EXPECT_TRUE(observer.wait_for("disconnected"));
    int16_t frames[160] = {};
    EXPECT_EQ(transport.send_user_audio(frames, 160), 0);
    EXPECT_EQ(transport.read_bot_audio(frames, 160), 0);
}

TEST(WebSocketTransport, DisconnectingIsNotReported) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    transport.disconnect();

    EXPECT_TRUE(bot.wait_for_closed());
    EXPECT_TRUE(observer.events().empty());
    int16_t frames[160] = {};
    EXPECT_EQ(transport.read_bot_audio(frames, 160), 0);
}

TEST(WebSocketTransport, ReportsInvalidFramesFromTheBot) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});

    bot.send("\x0f");
    EXPECT_TRUE(observer.wait_for("error:Invalid frame from the bot"));

    bot.send(websocket::encode_message("not json"));
    EXPECT_TRUE(observer.wait_for("error:Invalid message from the bot"));
}

TEST(WebSocketTransport, ConnectsAgainAfterTheBotDisconnects) {
    FakeBot bot;
    Observer observer;
    WebSocketTransport transport;
    transport.initialize(&observer);
    transport.connect({{"wsUrl", bot.url()}});
    bot.wait_for_client()->close();
    ASSERT_TRUE(observer.wait_for("disconnected"));

    FakeBot other_bot;
    transport.connect({{"wsUrl", other_bot.url()}});

    int16_t frames[160] = {};
    EXPECT_EQ(transport.send_user_audio(frames, 160), 160);
    EXPECT_TRUE(other_bot.wait_for_frame());
}
