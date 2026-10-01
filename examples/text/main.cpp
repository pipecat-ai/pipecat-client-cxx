//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

// Chats with a Pipecat bot in the terminal: starts the bot, sends each line you
// type and prints the bot's answers as they stream in.
//
// Usage: text_chat [--transport TRANSPORT] START_URL
//
// START_URL is the bot's start endpoint, e.g. http://localhost:7860/start for
// a local bot or https://api.pipecat.daily.co/v1/public/AGENT/start for
// Pipecat Cloud. If PIPECAT_API_KEY is set, it's sent as a bearer token.
//
// TRANSPORT is how to connect to the bot: `daily` (the default) or
// `websocket`, if the example was built with it.

#include <pipecat/pipecat.h>

#ifdef PIPECAT_EXAMPLE_DAILY
#include <pipecat/daily/transport.h>
#endif
#ifdef PIPECAT_EXAMPLE_WEBSOCKET
#include <pipecat/websocket/transport.h>
#endif

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace {

// Callbacks run on the client's own thread, so printing needs a lock.
std::mutex print_mutex;

void print(const std::string& text, bool new_line = true) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << text;
    if (new_line) {
        std::cout << std::endl;
    } else {
        std::cout << std::flush;
    }
}

class App : public pipecat::PipecatClientCallbacks {
   public:
    void on_bot_ready(const pipecat::rtvi::BotReadyData&) override {
        print("The bot is ready. Type a message and press Enter (Ctrl+D quits)."
        );
    }

    // The bot answers in text only (see main()), so print its LLM's text as
    // it streams in.
    void on_bot_llm_text(const pipecat::rtvi::BotLLMTextData& data) override {
        if (!_answering) {
            _answering = true;
            print("Bot: ", false);
        }
        print(data.text, false);
    }

    void on_bot_llm_stopped() override {
        if (_answering) {
            _answering = false;
            print("");
        }
    }

    void on_error(const pipecat::rtvi::ErrorData& error) override {
        print("Error: " + error.error);
    }

    void on_disconnected() override { print("Disconnected."); }

   private:
    bool _answering = false;
};

// Creates the transport called `name`, if the example was built with it, and
// asks the start endpoint for a bot that uses it.
std::unique_ptr<pipecat::Transport>
make_transport(const std::string& name, pipecat::APIRequest& request) {
#ifdef PIPECAT_EXAMPLE_DAILY
    if (name == "daily") {
        request.request_data = {{"createDailyRoom", true}};
        return std::make_unique<pipecat::DailyTransport>();
    }
#endif
#ifdef PIPECAT_EXAMPLE_WEBSOCKET
    if (name == "websocket") {
        request.request_data = {{"transport", "websocket"}};
        return std::make_unique<pipecat::WebSocketTransport>();
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

    // Answer in text only, since this example doesn't play audio.
    pipecat::rtvi::SendTextOptions text_options;
    text_options.audio_response = false;

    std::string line;
    while (client.connected() && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            client.send_text(line, text_options);
        } catch (const pipecat::PipecatError& e) {
            print(std::string("Unable to send: ") + e.what());
        }
    }

    client.disconnect();
    return EXIT_SUCCESS;
}
