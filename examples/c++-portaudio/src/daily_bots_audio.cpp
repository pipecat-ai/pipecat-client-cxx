//
// Copyright (c) 2024, Daily
//

#include "daily_bots.h"

#include <portaudio.h>

#include <signal.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <mutex>

static int audio_input_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long num_frames,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* user_data
) {
    rtvi::RTVIClient* client = (rtvi::RTVIClient*)user_data;

    client->send_user_audio((const int16_t*)input_buffer, num_frames);

    return paContinue;
}

class AudioOutput {
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

public:
    explicit AudioOutput(const uint32_t sample_rate) :
            _output_stream(nullptr),
            _buffer_mutex(),
            _playing(false),
            _buffer() {

         PaError err = Pa_OpenDefaultStream(
                &_output_stream,
                0,
                1,
                paInt16,
                sample_rate,
                paFramesPerBufferUnspecified,
                &pa_audio_output_callback,
                this
        );

        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }

        err = Pa_StartStream(_output_stream);

        if (err != paNoError) {
            Pa_CloseStream(_output_stream);
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }
    }

    void write_data(const int16_t* frames, const size_t num_frames) {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _buffer.insert(_buffer.end(), frames, frames + num_frames);
    }

    ~AudioOutput() {
        Pa_StopStream(_output_stream);
        Pa_CloseStream(_output_stream);
    }

private:
    static int pa_audio_output_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long num_frames,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* user_data
    ) {
        return static_cast<AudioOutput*>(user_data)->audio_output_callback(
            output_buffer,
            num_frames
        );
    }

    int audio_output_callback(
        void* output_buffer,
        unsigned long num_frames
    ) {
        std::lock_guard<std::mutex> lock(_buffer_mutex);

        int16_t* output_buffer_i16 = static_cast<int16_t*>(output_buffer);

        if (!_playing) {
            const size_t MIN_STARTING_FRAMES = std::max<size_t>(num_frames * 2, 240);

            if (_buffer.size() >= MIN_STARTING_FRAMES) {
                std::cout << "Starting playing" << std::endl;
                _playing = true;
            } else {
                memset(output_buffer_i16, 0, num_frames * 2);
                return paContinue;
            }
        }

        const size_t frames_remaining = _buffer.size();

        const size_t frames_to_copy = std::min<size_t>(num_frames, frames_remaining);

        std::copy(_buffer.begin(), _buffer.begin() + frames_to_copy, output_buffer_i16);
        _buffer.erase(_buffer.begin(), _buffer.begin() + frames_to_copy);

        if (frames_to_copy < num_frames) {
            std::cout << "End of buffered data, stopping" << std::endl;
            memset(output_buffer_i16 + frames_to_copy, 0, (num_frames - frames_to_copy) * 2);
            _playing = false;
        }

        return paContinue;
    }

private:
    PaStream* _output_stream;
    std::mutex _buffer_mutex;
    bool _playing;
    std::deque<int16_t> _buffer;
};

class AppMedia {
   public:
    AppMedia(rtvi::RTVIClient* client) : _client(client) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }
    }

    virtual ~AppMedia() { Pa_Terminate(); }

    void start() {

        _output.emplace(48000);

        PaError err = Pa_OpenDefaultStream(
                &_input_stream,
                1,
                0,
                paInt16,
                16000,
                320,
                audio_input_callback,
                _client
        );
        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }
        err = Pa_StartStream(_input_stream);
        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }
   }

    void stop() {
        PaError err = Pa_StopStream(_input_stream);
        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }

        err = Pa_CloseStream(_input_stream);
        if (err != paNoError) {
            throw std::runtime_error(
                    "PortAudio error: " + std::string(Pa_GetErrorText(err))
            );
        }

        _output.reset();
    }

    void write_audio(
            const int16_t* frames,
            size_t num_frames
    ) {
        _output->write_data(frames, num_frames);
    }

   private:
    PaStream* _input_stream;
    std::optional<AudioOutput> _output;
    rtvi::RTVIClient* _client;
};

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

        _media = std::make_unique<AppMedia>(_client.get());
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
        _media->start();
    }

    void on_disconnected() override {
        std::cout << std::endl << ">>> Client disconnected" << std::endl;
        _media->stop();
    }

    void on_user_started_speaking() override {
        std::cout << std::endl << ">>> User started speaking" << std::endl;
    }

    void on_user_stopped_speaking() override {
        std::cout << std::endl << ">>> User stopped speaking" << std::endl;
    }

    void on_user_transcript(const rtvi::UserTranscriptData& data) override {
        if (data.final) {
            std::cout << std::endl
                      << ">>> User transcript: " << data.text << std::endl;
        }
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
        _media->write_audio(frames, num_frames);
    }

   private:
    std::atomic<bool> _running;
    std::unique_ptr<AppMedia> _media;
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
    std::cout << "Usage: daily-bots-audio -b BASE_URL -c CONFIG_FILE"
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
