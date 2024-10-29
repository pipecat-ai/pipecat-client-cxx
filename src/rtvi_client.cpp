//
// Copyright (c) 2024, Daily
//

#include "rtvi_client.h"
#include "rtvi_exceptions.h"

#include <curl/curl.h>

using namespace rtvi;

RTVIClient::RTVIClient(
        const RTVIClientOptions& options,
        std::unique_ptr<RTVITransport> transport
)
    : _initialized(false),
      _connected(false),
      _options(options),
      _transport(std::move(transport)) {}

RTVIClient::~RTVIClient() {
    disconnect();
}

void RTVIClient::initialize() {
    if (_initialized) {
        return;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    _transport->initialize();

    _initialized = true;
}

void RTVIClient::connect() {
    if (!_initialized) {
        throw RTVIException("client is not initialized");
    }
    if (_connected) {
        return;
    }

    nlohmann::json response;
    try {
        response = connect_to_endpoint(
                _options.params.endpoints.connect,
                _options.params.request,
                _options.params.headers
        );
    } catch (nlohmann::json::parse_error& ex) {
        throw RTVIException(
                "unable to parse endpoint: " + std::string(ex.what())
        );
    }

    _transport->connect(response);

    _connected = true;
}

void RTVIClient::disconnect() {
    if (!_connected) {
        return;
    }

    _transport->disconnect();

    _connected = false;
}

void RTVIClient::send_action(const nlohmann::json& action) {
    if (!_connected) {
        return;
    }

    _transport->send_message(action);
}

void RTVIClient::send_action(
        const nlohmann::json& action,
        RTVIActionCallback callback
) {
    if (!_connected) {
        return;
    }

    std::unique_lock<std::mutex> lock(_actions_mutex);
    std::string action_id = action["id"].get<std::string>();
    _action_callbacks[action_id] = callback;
    lock.unlock();

    _transport->send_message(action);
}

int32_t RTVIClient::send_user_audio(const int16_t* frames, size_t num_frames) {
    if (!_connected) {
        return 0;
    }

    return _transport->send_user_audio(frames, num_frames);
}

int32_t RTVIClient::read_bot_audio(int16_t* frames, size_t num_frames) {
    if (!_connected) {
        return 0;
    }
    return _transport->read_bot_audio(frames, num_frames);
}

void RTVIClient::register_helper(
        const std::string& service,
        std::shared_ptr<RTVIHelper> helper
) {
    std::unique_lock<std::mutex> lock(_helpers_mutex);
    _helpers[service] = helper;
}

void RTVIClient::unregister_helper(const std::string& service) {
    std::unique_lock<std::mutex> lock(_helpers_mutex);
    _helpers.erase(service);
}

void RTVIClient::on_transport_message(const nlohmann::json& message) {
    auto type = message["type"].get<std::string>();
    switch (hash(type.c_str())) {
    case hash("action-response"):
        on_action_response(message);
        break;
    case hash("error-response"):
        if (_options.callbacks) {
            _options.callbacks->on_message_error(message);
        }
        break;
    case hash("bot-ready"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_ready();
        }
        break;
    case hash("bot-started-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_started_speaking();
        }
        break;
    case hash("bot-stopped-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_stopped_speaking();
        }
        break;
    // `tts-text`: RTVI 0.1.0 backwards compatibilty
    case hash("tts-text"):
    case hash("bot-transcription"): {
        if (_options.callbacks) {
            auto bot_data = BotTranscriptData {
                    .text = message["data"]["text"].get<std::string>()
            };
            _options.callbacks->on_bot_transcript(bot_data);
        }
        break;
    }
    case hash("bot-tts-started"): {
        if (_options.callbacks) {
            _options.callbacks->on_bot_tts_started();
        }
        break;
    }
    case hash("bot-tts-stopped"): {
        if (_options.callbacks) {
            _options.callbacks->on_bot_tts_stopped();
        }
        break;
    }
    case hash("bot-tts-text"): {
        if (_options.callbacks) {
            auto bot_data = BotTTSTextData {
                    .text = message["data"]["text"].get<std::string>()
            };
            _options.callbacks->on_bot_tts_text(bot_data);
        }
        break;
    }
    case hash("bot-llm-started"): {
        if (_options.callbacks) {
            _options.callbacks->on_bot_llm_started();
        }
        break;
    }
    case hash("bot-llm-stopped"): {
        if (_options.callbacks) {
            _options.callbacks->on_bot_llm_stopped();
        }
        break;
    }
    case hash("bot-llm-text"): {
        if (_options.callbacks) {
            auto bot_data = BotLLMTextData {
                    .text = message["data"]["text"].get<std::string>()
            };
            _options.callbacks->on_bot_llm_text(bot_data);
        }
        break;
    }
    case hash("user-started-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_user_started_speaking();
        }
        break;
    case hash("user-stopped-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_user_stopped_speaking();
        }
        break;
    case hash("user-transcription"): {
        if (_options.callbacks) {
            auto bot_data = UserTranscriptData {
                    .text = message["data"]["text"].get<std::string>(),
                    .final = message["data"]["final"].get<bool>(),
                    .timestamp =
                            message["data"]["timestamp"].get<std::string>(),
                    .user_id = message["data"]["user_id"].get<std::string>()
            };
            _options.callbacks->on_user_transcript(bot_data);
        }
        break;
    }
    default: {
        std::unique_lock<std::mutex> lock(_helpers_mutex);
        bool handled = false;
        for (const auto& [service, helper]: _helpers) {
            auto supported = helper->supported_messages();
            if (std::find(supported.begin(), supported.end(), type) !=
                supported.end()) {
                helper->handle_message(_transport.get(), message);
                handled = true;
            }
        }

        if (!handled && _options.callbacks) {
            _options.callbacks->on_generic_message(message);
        }
    }
    }
}

// Private

static size_t write_response_callback(
        void* contents,
        size_t size,
        size_t nmemb,
        std::string* output
) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

nlohmann::json RTVIClient::connect_to_endpoint(
        const std::string& url,
        const nlohmann::json& body,
        const std::vector<std::string>& headers
) const {
    std::string response_body;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw RTVIException("unable to initialize CURL");
    }

    struct curl_slist* curl_headers = nullptr;

    for (const std::string& header: headers) {
        curl_headers = curl_slist_append(curl_headers, header.c_str());
    }
    curl_headers =
            curl_slist_append(curl_headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    std::string body_json = body.dump();
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    CURLcode res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
        throw RTVIException(
                "unable to perform POST request: " +
                std::string(curl_easy_strerror(res))
        );
    }

    long status;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (status != 200) {
        throw RTVIException(
                "unable to perform POST request (status: " +
                std::to_string(status) + "): " + response_body
        );
    }

    curl_easy_cleanup(curl);

    return nlohmann::json::parse(response_body);
}

void RTVIClient::on_action_response(const nlohmann::json& response) {
    std::unique_lock<std::mutex> lock(_actions_mutex);
    std::string action_id = response["id"].get<std::string>();
    RTVIActionCallback action_callback = nullptr;
    if (_action_callbacks.find(action_id) != _action_callbacks.end()) {
        action_callback = _action_callbacks[action_id];
        _action_callbacks.erase(action_id);
    }
    lock.unlock();

    if (action_callback) {
        action_callback(response["data"]);
    }
}
