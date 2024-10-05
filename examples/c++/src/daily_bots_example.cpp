//
// Copyright (c) 2024, Daily
//

#include "daily_bots.h"

#include <signal.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

class App : public rtvi::RTVIEventCallbacks {
   public:
    App(const std::string& url, const nlohmann::json& config) : _running(true) {
        std::optional<std::string> api_key;
        const char* api_key_env = std::getenv("DAILY_BOTS_API_KEY");
        if (api_key_env) {
            api_key = std::string(api_key_env);
        }

        auto params = rtvi::RTVIClientParams {
                .base_url = url, .api_key = api_key, .request = config
        };

        auto options =
                rtvi::RTVIClientOptions {.params = params, .callbacks = this};

        auto transport = std::make_unique<daily::DailyBotsTransport>(options);

        _client = std::make_unique<rtvi::RTVIClient>(
                options, std::move(transport)
        );
    }

    virtual ~App() {}

    void run() {
        _client->initialize();
        _client->connect();

        while (_running) {
            sleep(1);
        }

        _client->disconnect();
    }

    void quit() { _running = false; }

    // RTVIEventCallbacks

    void on_connected() override {
        std::cout << std::endl << ">>> Client connected" << std::endl;
    }

    void on_disconnected() override {
        std::cout << std::endl << ">>> Client disconnected" << std::endl;
    }

    void on_bot_connected(const nlohmann::json& bot) override {
        std::cout << std::endl << ">>> Bot connected" << std::endl;
    }

    void on_bot_disconnected(
            const nlohmann::json& bot,
            const std::string& reason
    ) override {
        std::cout << std::endl
                  << ">>> Bot disconnected: " << reason << std::endl;
    }

    void on_bot_ready() override {
        std::cout << std::endl << ">>> Bot ready" << std::endl;

        // Send an initial action as an example.
        nlohmann::json action = rtvi::RTVIMessage::action(
                "llm", "append_to_messages", nlohmann::json::parse(R"([
                  {
                    "name": "messages",
                    "value": [{
                      "role": "system",
                      "content": "Tell me a story about cats and dragons."
                    }]
                  },
                  { "name": "run_immediately", "value": true }
                ])")
        );

        _client->send_action(action);
    }

    void on_bot_started_speaking(const nlohmann::json& bot) override {
        std::cout << std::endl << ">>> Bot started speaking" << std::endl;
    }

    void on_bot_stopped_speaking(const nlohmann::json& bot) override {
        std::cout << std::endl << ">>> Bot stopped speaking" << std::endl;
    }

    void on_bot_tts_text(const rtvi::BotTTSTextData& data) override {
        std::cout << std::endl
                  << ">>> Bot TTS text: " << data.text << std::endl;
    }

    void on_bot_llm_text(const rtvi::BotLLMTextData& data) override {
        std::cout << std::endl
                  << ">>> Bot LLM text: " << data.text << std::endl;
    }

    void on_bot_audio(
            const int16_t* frames,
            size_t num_frames,
            uint32_t sample_rate,
            uint32_t num_channels
    ) override {
        // Audio from the bot is received in this callback.
    }

   private:
    std::atomic<bool> _running;
    std::unique_ptr<rtvi::RTVIClient> _client;
};

static std::unique_ptr<App> app;

static void signal_handler(int signum) {
    std::cout << std::endl;
    std::cout << "Interrupted!" << std::endl;
    std::cout << std::endl;
    if (app) {
        app->quit();
    }
}

static void usage() {
    std::cout << "Usage: daily-bots-example -b BASE_URL -c CONFIG_FILE"
              << std::endl;
    std::cout << "  -b    Daily Bots base URL" << std::endl;
    std::cout << "  -c    Configuration file" << std::endl;
}

int main(int argc, char* argv[]) {
    char* url = nullptr;
    char* config_file = nullptr;

    int opt;
    while ((opt = getopt(argc, argv, "b:c:")) != -1) {
        switch (opt) {
        case 'b':
            url = strdup(optarg);
            break;
        case 'c':
            config_file = strdup(optarg);
            break;
        default:
            usage();
            return EXIT_FAILURE;
        }
    }

    if (optind < argc || url == nullptr || config_file == nullptr) {
        usage();
        return EXIT_SUCCESS;
    }

    std::ifstream input_file(config_file);
    if (!input_file.is_open()) {
        std::cerr << std::endl
                  << "ERROR: unable to open config file: " << config_file
                  << std::endl;
        return EXIT_FAILURE;
    }

    nlohmann::json config_json;

    try {
        input_file >> config_json;
        input_file.close();
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << std::endl
                  << "ERROR: invalid config file: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        app = std::make_unique<App>(url, config_json);
        app->run();
    } catch (const std::exception& ex) {
        std::cerr << std::endl
                  << "ERROR: error running the app: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
