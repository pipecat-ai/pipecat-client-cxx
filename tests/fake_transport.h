//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_TESTS_FAKE_TRANSPORT_H
#define PIPECAT_TESTS_FAKE_TRANSPORT_H

#include "event_loop.h"

#include "pipecat/errors.h"
#include "pipecat/transport.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

// Builds an RTVI message as the bot would send it.
inline nlohmann::json rtvi_message(
        const std::string& type,
        const nlohmann::json& data = nullptr,
        const std::string& id = ""
) {
    nlohmann::json message = {{"label", "rtvi-ai"}, {"type", type}};
    if (!data.is_null()) {
        message["data"] = data;
    }
    if (!id.empty()) {
        message["id"] = id;
    }
    return message;
}

// A transport that behaves like daily-core: it reports everything on a
// single event thread, and connect() and disconnect() block until that
// thread completes them. With daily-core, calling them from the event thread
// deadlocks. Here they throw and set `deadlock_detected` instead.
class FakeTransport : public pipecat::Transport {
   public:
    inline static const pipecat::Participant BOT {"bot-id", "Bot", false};

    // Throw from connect().
    bool fail_connect = false;
    // Answer client-ready with bot-ready.
    bool send_bot_ready = true;
    std::string bot_version = "2.1.0";
    size_t max_size = 64 * 1024;
    // Optional hooks, run when disconnect() or send_message() start, on the
    // caller's thread.
    std::function<void()> on_disconnect;
    std::function<void()> on_send_message;

    std::atomic<bool> deadlock_detected {false};
    std::atomic<int> initialize_count {0};
    std::atomic<int> disconnect_count {0};

    void initialize(pipecat::TransportObserver* observer) override {
        _observer = observer;
        initialize_count++;
    }

    void connect(const nlohmann::json& params) override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _connect_params = params;
        }
        if (fail_connect) {
            throw pipecat::TransportStartError("Fake connect failure");
        }
        complete_on_event_thread();
        _connected = true;
    }

    void disconnect() override {
        if (!_connected) {
            return;
        }
        if (on_disconnect) {
            on_disconnect();
        }
        complete_on_event_thread();
        _connected = false;
        disconnect_count++;
    }

    void send_ready_message(const pipecat::rtvi::Message& message) override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _ready_message = message;
        }
        if (send_bot_ready) {
            deliver_message(
                    rtvi_message("bot-ready", {{"version", bot_version}})
            );
        }
    }

    void send_message(const pipecat::rtvi::Message& message) override {
        if (on_send_message) {
            on_send_message();
        }
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sent_messages.push_back(message);
        }
        _sent_cv.notify_all();
    }

    size_t max_message_size() const override { return max_size; }

    int32_t send_user_audio(const int16_t*, size_t num_frames) override {
        return static_cast<int32_t>(num_frames);
    }

    int32_t read_bot_audio(int16_t*, size_t num_frames) override {
        return static_cast<int32_t>(num_frames);
    }

    //
    // Events, reported on the event thread like daily-core does.
    //

    void deliver_message(const nlohmann::json& message) {
        _events.post([this, message] {
            _observer->on_transport_message(message);
        });
    }

    void deliver_bot_connected() {
        _events.post([this] { _observer->on_bot_connected(BOT); });
    }

    void deliver_bot_disconnected() {
        _events.post([this] { _observer->on_bot_disconnected(BOT); });
    }

    void deliver_participant_joined(const pipecat::Participant& participant) {
        _events.post([this, participant] {
            _observer->on_participant_joined(participant);
        });
    }

    void deliver_participant_left(const pipecat::Participant& participant) {
        _events.post([this, participant] {
            _observer->on_participant_left(participant);
        });
    }

    void deliver_transport_disconnected() {
        _events.post([this] {
            _connected = false;
            _observer->on_transport_disconnected();
        });
    }

    // Waits until the event thread has handled everything reported so far.
    void flush() { complete_on_event_thread(); }

    //
    // Inspection
    //

    bool connected() const { return _connected; }

    nlohmann::json connect_params() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _connect_params;
    }

    std::optional<pipecat::rtvi::Message> ready_message() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _ready_message;
    }

    // Messages sent to the bot, optionally only those of a type.
    std::vector<pipecat::rtvi::Message> sent_messages(
            const std::string& type = ""
    ) {
        std::lock_guard<std::mutex> lock(_mutex);
        return sent_messages_locked(type);
    }

    // Waits until `count` messages of `type` were sent. Returns false after a
    // timeout.
    bool wait_for_sent(const std::string& type, size_t count = 1) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _sent_cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return sent_messages_locked(type).size() >= count;
        });
    }

   private:
    std::vector<pipecat::rtvi::Message> sent_messages_locked(
            const std::string& type
    ) {
        std::vector<pipecat::rtvi::Message> messages;
        for (const auto& message: _sent_messages) {
            if (type.empty() || message.type == type) {
                messages.push_back(message);
            }
        }
        return messages;
    }

    // Blocks until the event thread runs a completion, like daily-core's
    // request-completed events.
    void complete_on_event_thread() {
        if (_events.in_loop_thread()) {
            deadlock_detected = true;
            throw std::logic_error(
                    "Blocking transport call on the transport's event thread"
            );
        }
        auto done = std::make_shared<std::promise<void>>();
        auto future = done->get_future();
        _events.post([done] { done->set_value(); });
        if (future.wait_for(std::chrono::seconds(5)) !=
            std::future_status::ready) {
            throw std::runtime_error("Timed out waiting for the event thread");
        }
    }

    pipecat::TransportObserver* _observer = nullptr;
    std::atomic<bool> _connected {false};

    std::mutex _mutex;
    std::condition_variable _sent_cv;
    nlohmann::json _connect_params;
    std::optional<pipecat::rtvi::Message> _ready_message;
    std::vector<pipecat::rtvi::Message> _sent_messages;

    // Last, so it stops first and its tasks never see destroyed members.
    pipecat::EventLoop _events;
};

#endif
