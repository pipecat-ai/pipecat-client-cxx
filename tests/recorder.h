//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_TESTS_RECORDER_H
#define PIPECAT_TESTS_RECORDER_H

#include "pipecat/client.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Records client callbacks as strings (e.g. "bot-output:Hello"), and the
// thread they ran on.
class Recorder : public pipecat::PipecatClientCallbacks {
   public:
    // Optional hooks, run inside the callback.
    std::function<void()> on_disconnected_hook;
    std::function<void()> on_bot_disconnected_hook;
    std::function<void()> on_user_started_speaking_hook;

    std::vector<std::string> events() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _events;
    }

    // Events starting with `prefix`, in order.
    std::vector<std::string> events(const std::string& prefix) const {
        std::vector<std::string> matching;
        for (const auto& event: events()) {
            if (event.rfind(prefix, 0) == 0) {
                matching.push_back(event);
            }
        }
        return matching;
    }

    // Waits until `event` is recorded. Returns false after a timeout.
    bool wait_for(
            const std::string& event,
            std::chrono::milliseconds timeout = std::chrono::seconds(5)
    ) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _cv.wait_for(lock, timeout, [&] {
            return std::find(_events.begin(), _events.end(), event) !=
                   _events.end();
        });
    }

    // Threads callbacks ran on.
    std::vector<std::thread::id> threads() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _threads;
    }

    // Connection

    void on_transport_state_changed(pipecat::TransportState state) override {
        record(std::string("state:") + pipecat::to_string(state));
    }
    void on_connected() override { record("connected"); }
    void on_disconnected() override {
        record("disconnected");
        if (on_disconnected_hook) {
            on_disconnected_hook();
        }
    }
    void on_error(const pipecat::rtvi::ErrorData& error) override {
        record("error:" + error.error + (error.fatal ? ":fatal" : ""));
    }

    // Bot and participants

    void on_bot_started(const nlohmann::json& response) override {
        record("bot-started:" + response.dump());
    }
    void on_bot_connected(const pipecat::Participant& bot) override {
        record("bot-connected:" + bot.id);
    }
    void on_bot_ready(const pipecat::rtvi::BotReadyData& data) override {
        record("bot-ready:" + data.version);
    }
    void on_bot_disconnected(const pipecat::Participant& bot) override {
        record("bot-disconnected:" + bot.id);
        if (on_bot_disconnected_hook) {
            on_bot_disconnected_hook();
        }
    }
    void on_participant_joined(const pipecat::Participant& participant
    ) override {
        record("participant-joined:" + participant.id);
    }
    void on_participant_left(const pipecat::Participant& participant) override {
        record("participant-left:" + participant.id);
    }

    // Messages

    void on_server_message(const nlohmann::json& data) override {
        record("server-message:" + data.dump());
    }
    void on_message_error(const pipecat::rtvi::ErrorData& error) override {
        record("message-error:" + error.error);
    }
    void on_metrics(const pipecat::rtvi::MetricsData& data) override {
        record("metrics:" + std::to_string(data.ttfb.size()));
    }
    void on_unhandled_message(const pipecat::rtvi::Message& message) override {
        record("unhandled:" + message.type);
    }

    // Speaking

    void on_user_started_speaking() override {
        record("user-started-speaking");
        if (on_user_started_speaking_hook) {
            on_user_started_speaking_hook();
        }
    }
    void on_user_stopped_speaking() override {
        record("user-stopped-speaking");
    }
    void on_bot_started_speaking() override { record("bot-started-speaking"); }
    void on_bot_stopped_speaking() override { record("bot-stopped-speaking"); }
    void on_user_mute_started() override { record("user-mute-started"); }
    void on_user_mute_stopped() override { record("user-mute-stopped"); }

    // Transcription and output

    void on_user_transcript(const pipecat::rtvi::TranscriptData& data
    ) override {
        record("user-transcript:" + data.text + (data.final ? ":final" : ""));
    }
    void on_user_llm_text(const pipecat::rtvi::UserLLMTextData& data) override {
        record("user-llm-text:" + data.text);
    }
    void on_bot_output(const pipecat::rtvi::BotOutputData& data) override {
        record("bot-output:" + data.text + ":" + data.aggregated_by);
    }

    // LLM

    void on_bot_llm_started() override { record("bot-llm-started"); }
    void on_bot_llm_stopped() override { record("bot-llm-stopped"); }
    void on_bot_llm_text(const pipecat::rtvi::BotLLMTextData& data) override {
        record("bot-llm-text:" + data.text);
    }
    void on_bot_llm_search_response(
            const pipecat::rtvi::BotLLMSearchResponseData& data
    ) override {
        record("bot-llm-search-response:" + data.search_result.value_or(""));
    }

    // Function calls

    void on_llm_function_call_started(
            const pipecat::rtvi::LLMFunctionCallStartedData& data
    ) override {
        record("function-call-started:" + data.function_name.value_or(""));
    }
    void on_llm_function_call_in_progress(
            const pipecat::rtvi::LLMFunctionCallInProgressData& data
    ) override {
        record("function-call-in-progress:" + data.function_name.value_or("") +
               ":" + data.arguments.dump());
    }
    void on_llm_function_call_stopped(
            const pipecat::rtvi::LLMFunctionCallStoppedData& data
    ) override {
        record("function-call-stopped:" + data.tool_call_id);
    }

    // TTS

    void on_bot_tts_started() override { record("bot-tts-started"); }
    void on_bot_tts_stopped() override { record("bot-tts-stopped"); }
    void on_bot_tts_text(const pipecat::rtvi::BotTTSTextData& data) override {
        record("bot-tts-text:" + data.text);
    }

   private:
    void record(const std::string& event) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _events.push_back(event);
            _threads.push_back(std::this_thread::get_id());
        }
        _cv.notify_all();
    }

    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::vector<std::string> _events;
    std::vector<std::thread::id> _threads;
};

#endif
