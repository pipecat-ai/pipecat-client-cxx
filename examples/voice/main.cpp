//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

// Talks with a Pipecat bot using the default microphone and speakers.
//
// Usage: voice_chat [--transport TRANSPORT] START_URL
//
// START_URL is the bot's start endpoint, e.g. http://localhost:7860/start for
// a local bot or https://api.pipecat.daily.co/v1/public/AGENT/start for
// Pipecat Cloud. If PIPECAT_API_KEY is set, it's sent as a bearer token.
//
// TRANSPORT is how to connect to the bot: `daily` (the default) or
// `websocket`, if the example was built with it.
//
// If the bot has a `get_current_time` function for the client to run, this
// example answers it.

#include "audio.h"

#include <pipecat/pipecat.h>

#ifdef PIPECAT_EXAMPLE_DAILY
#include <pipecat/daily/transport.h>
#endif
#ifdef PIPECAT_EXAMPLE_WEBSOCKET
#include <pipecat/websocket/transport.h>
#endif

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
#include <vector>

namespace {

const uint32_t SAMPLE_RATE = 16000;

std::atomic<bool> running {true};

// Callbacks run on the client's own thread, so printing needs a lock.
std::mutex print_mutex;

void print(const std::string& text) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << text << std::endl;
}

std::string current_time() {
    std::time_t now = std::time(nullptr);
    char text[64];
    std::strftime(text, sizeof(text), "%H:%M", std::localtime(&now));
    return text;
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

    // Answers the bot's `get_current_time` function calls.
    void on_llm_function_call_in_progress(
            const pipecat::rtvi::LLMFunctionCallInProgressData& data,
            pipecat::FunctionCallResultCallback respond
    ) override {
        if (data.function_name == "get_current_time") {
            respond({{"time", current_time()}});
        }
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

// Creates the transport called `name`, if the example was built with it, and
// asks the start endpoint for a bot that uses it.
std::unique_ptr<pipecat::Transport>
make_transport(const std::string& name, pipecat::APIRequest& request) {
#ifdef PIPECAT_EXAMPLE_DAILY
    if (name == "daily") {
        pipecat::DailyTransportOptions options;
        options.user_audio_sample_rate = SAMPLE_RATE;
        options.bot_audio_sample_rate = SAMPLE_RATE;
        request.request_data = {{"createDailyRoom", true}};
        return std::make_unique<pipecat::DailyTransport>(options);
    }
#endif
#ifdef PIPECAT_EXAMPLE_WEBSOCKET
    if (name == "websocket") {
        pipecat::WebSocketTransportOptions options;
        options.user_audio_sample_rate = SAMPLE_RATE;
        options.bot_audio_sample_rate = SAMPLE_RATE;
        request.request_data = {{"transport", "websocket"}};
        return std::make_unique<pipecat::WebSocketTransport>(options);
    }
#endif
    return nullptr;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string transport = "daily";
    if (args.size() == 3 && args[0] == "--transport") {
        transport = args[1];
        args.erase(args.begin(), args.begin() + 2);
    }
    if (args.size() != 1) {
        std::cerr << "Usage: " << argv[0]
                  << " [--transport TRANSPORT] START_URL" << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, [](int) { running = false; });

    pipecat::APIRequest request;
    request.endpoint = args[0];
    if (const char* api_key = std::getenv("PIPECAT_API_KEY")) {
        request.headers["Authorization"] = std::string("Bearer ") + api_key;
    }

    App app;

    pipecat::PipecatClientOptions options;
    options.transport = make_transport(transport, request);
    if (!options.transport) {
        std::cerr << "This example was built without the " << transport
                  << " transport" << std::endl;
        return EXIT_FAILURE;
    }
    options.callbacks = &app;
    pipecat::PipecatClient client(std::move(options));

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
