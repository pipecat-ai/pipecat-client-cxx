//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

// Talks with a Pipecat bot using the default microphone and speakers.
//
// Usage: voice_chat START_URL
//
// START_URL is the bot's start endpoint, e.g. http://localhost:7860/start for
// a local bot or https://api.pipecat.daily.co/v1/public/AGENT/start for
// Pipecat Cloud. If PIPECAT_API_KEY is set, it's sent as a bearer token.
//
// If the bot has a `get_current_time` function for the client to run, this
// example answers it.

#include "audio.h"

#include <pipecat/daily/transport.h>
#include <pipecat/pipecat.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace {

const uint32_t SAMPLE_RATE = 16000;

std::atomic<bool> running {true};

// Callbacks run on the client's own thread, so printing needs a lock.
std::mutex print_mutex;

void print(const std::string& text) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << text << std::endl;
}

class App : public pipecat::PipecatClientCallbacks {
   public:
    void on_bot_ready(const pipecat::rtvi::BotReadyData&) override {
        print("The bot is ready. Start talking (Ctrl+C quits).");
    }

    void on_user_transcript(const pipecat::rtvi::TranscriptData& data
    ) override {
        if (data.final) {
            print("You: " + data.text);
        }
    }

    void on_bot_output(const pipecat::rtvi::BotOutputData& data) override {
        // Text the bot speaks is sent again as it speaks it, so only print
        // the first update of each segment.
        if (data.segment_id && !_printed.insert(*data.segment_id).second) {
            return;
        }
        print("Bot: " + data.text);
    }

    void on_error(const pipecat::rtvi::ErrorData& error) override {
        print("Error: " + error.error);
    }

    void on_disconnected() override {
        print("Disconnected.");
        running = false;
    }

   private:
    std::set<int64_t> _printed;
};

std::string current_time() {
    std::time_t now = std::time(nullptr);
    char text[64];
    std::strftime(text, sizeof(text), "%H:%M", std::localtime(&now));
    return text;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " START_URL" << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, [](int) { running = false; });

    App app;

    pipecat::DailyTransportOptions transport_options;
    transport_options.user_audio_sample_rate = SAMPLE_RATE;
    transport_options.bot_audio_sample_rate = SAMPLE_RATE;

    pipecat::PipecatClientOptions options;
    options.transport =
            std::make_unique<pipecat::DailyTransport>(transport_options);
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

    client.register_function_call_handler(
            "get_current_time",
            [](const pipecat::FunctionCallParams&,
               pipecat::FunctionCallResultCallback respond) {
                respond({{"time", current_time()}});
            }
    );

    pipecat::APIRequest request;
    request.endpoint = argv[1];
    request.request_data = {{"createDailyRoom", true}};
    if (const char* api_key = std::getenv("PIPECAT_API_KEY")) {
        request.headers["Authorization"] = std::string("Bearer ") + api_key;
    }

    try {
        print("Starting the bot...");
        client.start_bot_and_connect(request);
    } catch (const pipecat::PipecatError& e) {
        std::cerr << "Unable to connect: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    try {
        Audio audio(client, SAMPLE_RATE);
        audio.start();
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // Disconnect first: it ends the bot's audio, so the speaker thread
        // finishes.
        client.disconnect();
        audio.stop();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
