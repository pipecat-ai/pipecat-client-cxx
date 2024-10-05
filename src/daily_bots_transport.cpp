//
// Copyright (c) 2024, Daily
//

#include "daily_bots.h"

using namespace daily;

// NOTE: Do not modify. This is a way for the server to recognize a known
// client library.
static DailyAboutClient ABOUT_CLIENT = {
        .library = "daily-bots-sdk",
        .version = "0.11.0"
};

// Simple hashing function so we can fake pattern matching and switch on strings
// as a constexpr so it gets evaluated in compile time for static strings
constexpr unsigned int hash(const char* s, int off = 0) {
    return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
}

static WebrtcAudioDeviceModule* create_audio_device_module_cb(
        DailyRawWebRtcContextDelegate* delegate,
        WebrtcTaskQueueFactory* task_queue_factory
) {
    auto transport = static_cast<DailyBotsTransport*>(delegate);

    return transport->create_audio_device_module(task_queue_factory);
}

static void* get_user_media_cb(
        DailyRawWebRtcContextDelegate* delegate,
        WebrtcPeerConnectionFactory* peer_connection_factory,
        WebrtcThread* webrtc_signaling_thread,
        WebrtcThread* webrtc_worker_thread,
        WebrtcThread* webrtc_network_thread,
        const char* constraints
) {
    auto transport = static_cast<DailyBotsTransport*>(delegate);

    return transport->get_user_media(
            peer_connection_factory,
            webrtc_signaling_thread,
            webrtc_worker_thread,
            webrtc_network_thread,
            constraints
    );
}

static char* enumerate_devices_cb(DailyRawWebRtcContextDelegate* delegate) {
    auto transport = static_cast<DailyBotsTransport*>(delegate);

    return transport->enumerate_devices();
}

static const char* get_audio_device_cb(DailyRawWebRtcContextDelegate* delegate
) {
    return "";
}

static void set_audio_device_cb(
        DailyRawWebRtcContextDelegate* delegate,
        const char* device_id
) {}

static void on_event_cb(
        DailyRawCallClientDelegate* delegate,
        const char* event_json,
        intptr_t json_len
) {
    nlohmann::json event = nlohmann::json::parse(event_json);
    auto transport = static_cast<DailyBotsTransport*>(delegate);

    transport->on_event(event);
}

static void on_audio_data_cb(
        DailyRawCallClientDelegate* delegate,
        uint64_t renderer_id,
        const char* peer_id,
        const struct NativeAudioData* audio_data
) {
    auto transport = static_cast<DailyBotsTransport*>(delegate);

    transport->on_audio_data(audio_data);
}

DailyBotsTransport::DailyBotsTransport(const rtvi::RTVIClientOptions& options)
    : _initialized(false),
      _connected(false),
      _options(options),
      _client(NULL),
      _request_id(0) {}

DailyBotsTransport::~DailyBotsTransport() {
    disconnect();
}

void DailyBotsTransport::initialize() {
    if (_initialized) {
        return;
    }

    daily_core_set_log_level(DailyLogLevel_Off);

    _device_manager = daily_core_context_create_device_manager();

    DailyContextDelegatePtr* ptr = nullptr;
    DailyContextDelegate driver = {.ptr = ptr};

    DailyWebRtcContextDelegate webrtc = {
            .ptr = (DailyWebRtcContextDelegatePtr*)this,
            .fns = {.get_user_media = get_user_media_cb,
                    .get_enumerated_devices = enumerate_devices_cb,
                    .create_audio_device_module = create_audio_device_module_cb,
                    .get_audio_device = get_audio_device_cb,
                    .set_audio_device = set_audio_device_cb}
    };

    daily_core_context_create(driver, webrtc, ABOUT_CLIENT);

    _microphone = daily_core_context_create_virtual_microphone_device(
            _device_manager, "mic", 16000, 1, false
    );

    _initialized = true;
}

void DailyBotsTransport::connect(const nlohmann::json& info) {
    if (!_initialized) {
        throw rtvi::RTVIException("transport is not initialized");
    }
    if (_connected) {
        return;
    }

    if (!info.is_object()) {
        throw rtvi::RTVIException("invalid connection info: not an object");
    }

    if (!info.contains("room_url") || !info.contains("token")) {
        throw rtvi::RTVIException(
                "invalid connection info: missing `room_url` or `token`"
        );
    }

    // Cleanup bot participant.
    _bot_participant = nullptr;

    std::string room_url = info["room_url"].get<std::string>();
    std::string token = info["token"].get<std::string>();

    _client = daily_core_call_client_create();

    DailyCallClientDelegate delegate = {
            .ptr = this,
            .fns = {.on_event = on_event_cb, .on_audio_data = on_audio_data_cb}
    };

    daily_core_call_client_set_delegate(_client, delegate);

    // Subscriptions profiles
    nlohmann::json profiles = nlohmann::json::parse(R"({
      "base": {
        "camera": "unsubscribed",
        "microphone": "subscribed"
      }
    })");
    std::string profiles_str = profiles.dump();

    std::promise<void> update_promise;
    std::future<void> update_future = update_promise.get_future();
    uint64_t request_id = add_completion(std::move(update_promise));
    daily_core_call_client_update_subscription_profiles(
            _client, request_id, profiles_str.c_str()
    );
    update_future.get();

    // Client settings
    nlohmann::json settings = nlohmann::json::parse(R"({
      "inputs": {
        "camera": false,
        "microphone": {
          "isEnabled": true,
          "settings": {
            "deviceId": "mic",
            "customConstraints": {
              "echoCancellation": { "exact": true }
            }
          }
        }
      }
    })");
    std::string settings_str = settings.dump();

    std::promise<void> join_promise;
    std::future<void> join_future = join_promise.get_future();
    request_id = add_completion(std::move(join_promise));
    daily_core_call_client_join(
            _client,
            request_id,
            room_url.c_str(),
            token.c_str(),
            settings_str.c_str()
    );
    join_future.get();

    // Start send message thread.
    _msg_thread = std::thread(&DailyBotsTransport::send_message_thread, this);

    _connected = true;

    if (_options.callbacks) {
        _options.callbacks->on_connected();
    }
}

void DailyBotsTransport::disconnect() {
    if (!_connected) {
        return;
    }

    // Stop and wait for send message thread to finish.
    _msg_queue.stop();
    _msg_thread.join();

    std::promise<void> leave_promise;
    std::future<void> leave_future = leave_promise.get_future();
    uint64_t request_id = add_completion(std::move(leave_promise));
    daily_core_call_client_leave(_client, request_id);
    leave_future.get();

    daily_core_call_client_destroy(_client);

    _connected = false;

    if (_options.callbacks) {
        _options.callbacks->on_disconnected();
    }
}

void DailyBotsTransport::send_message(const nlohmann::json& message) {
    if (!_connected) {
        return;
    }

    _msg_queue.push(message);
}

void DailyBotsTransport::send_action(const nlohmann::json& action) {
    send_message(action);
}

void DailyBotsTransport::send_action(
        const nlohmann::json& action,
        rtvi::RTVIActionCallback callback
) {
    std::unique_lock<std::mutex> lock(_actions_mutex);
    std::string action_id = action["id"].get<std::string>();
    _action_callbacks[action_id] = callback;
    lock.unlock();

    send_message(action);
}

int32_t
DailyBotsTransport::send_user_audio(const int16_t* frames, size_t num_frames) {
    if (!_connected) {
        return 0;
    }

    return daily_core_context_virtual_microphone_device_write_frames(
            _microphone, frames, num_frames, _request_id++, NULL, NULL
    );
}

// Public but internal

WebrtcAudioDeviceModule* DailyBotsTransport::create_audio_device_module(
        WebrtcTaskQueueFactory* task_queue_factory
) {
    return daily_core_context_create_audio_device_module(
            _device_manager, task_queue_factory
    );
}

char* DailyBotsTransport::enumerate_devices() {
    return daily_core_context_device_manager_enumerated_devices(_device_manager
    );
}

void* DailyBotsTransport::get_user_media(
        WebrtcPeerConnectionFactory* peer_connection_factory,
        WebrtcThread* webrtc_signaling_thread,
        WebrtcThread* webrtc_worker_thread,
        WebrtcThread* webrtc_network_thread,
        const char* constraints
) {
    return daily_core_context_device_manager_get_user_media(
            _device_manager,
            peer_connection_factory,
            webrtc_signaling_thread,
            webrtc_worker_thread,
            webrtc_network_thread,
            constraints
    );
}

void DailyBotsTransport::on_event(const nlohmann::json& event) {
    auto action = event["action"].get<std::string>();

    switch (hash(action.c_str())) {
    case hash("participant-joined"):
        on_participant_joined(event["participant"]);
        break;
    case hash("participant-updated"):
        on_participant_updated(event["participant"]);
        break;
    case hash("participant-left"):
        on_participant_left(
                event["participant"], event["leftReason"].get<std::string>()
        );
        break;
    case hash("app-message"): {
        if (event.contains("msgData") && event["msgData"].contains("label")) {
            auto label = event["msgData"]["label"].get<std::string>();
            if (label == "rtvi-ai") {
                on_rtvi_message(event["msgData"]);
            }
        }

        break;
    }
    case hash("error"):
        break;
    case hash("request-completed"): {
        auto request_id = event["requestId"]["id"].get<uint64_t>();
        resolve_completion(request_id);
        break;
    }
    default:
        break;
    }
}

void DailyBotsTransport::on_audio_data(const struct NativeAudioData* audio_data
) {
    if (_options.callbacks) {
        _options.callbacks->on_bot_audio(
                (const int16_t*)audio_data->audio_frames,
                audio_data->num_audio_frames,
                audio_data->sample_rate,
                audio_data->num_channels
        );
    }
}

// Private

uint64_t DailyBotsTransport::add_completion(std::promise<void> completion) {
    std::lock_guard<std::mutex> lock(_completions_mutex);

    uint64_t request_id = _request_id++;
    _completions[request_id] = std::move(completion);

    return request_id;
}

void DailyBotsTransport::resolve_completion(uint64_t request_id) {
    std::lock_guard<std::mutex> lock(_completions_mutex);
    _completions[request_id].set_value();
    _completions.erase(request_id);
}

void DailyBotsTransport::send_message_thread() {
    bool running = true;
    while (running) {
        std::optional<nlohmann::json> message = _msg_queue.blocking_pop();
        if (message.has_value()) {
            std::string data = (*message).dump();

            std::promise<void> msg_promise;
            std::future<void> msg_future = msg_promise.get_future();
            uint64_t request_id = add_completion(std::move(msg_promise));
            daily_core_call_client_send_app_message(
                    _client, request_id, data.c_str(), NULL
            );
            msg_future.get();
        } else {
            running = false;
        }
    }
}

void DailyBotsTransport::on_participant_joined(const nlohmann::json& participant
) {
    std::string participant_id = participant["id"].get<std::string>();

    // We assume the first remote participant is a bot.
    if (_bot_participant.is_null()) {
        _bot_participant = participant;

        if (_options.callbacks) {
            _options.callbacks->on_bot_connected(participant);
        }
    }
}

void DailyBotsTransport::on_participant_updated(
        const nlohmann::json& participant
) {
    std::string participant_id = participant["id"].get<std::string>();
    bool is_local = participant["info"]["isLocal"].get<bool>();

    // We don't care about local updates, at least for now. Also, make sure we
    // have a bot participant (we should if hte update is non-local).
    if (is_local || _bot_participant.is_null()) {
        return;
    }

    std::string bot_participant_id = _bot_participant["id"].get<std::string>();

    // Let's make sure the updated participant is the bot one.
    if (participant_id == bot_participant_id) {
        std::string mic_state =
                participant["media"]["microphone"]["state"].get<std::string>();

        if (mic_state == "playable") {
            daily_core_call_client_set_participant_audio_renderer(
                    _client,
                    _request_id++,
                    0,
                    participant_id.c_str(),
                    "microphone"
            );

            send_message(rtvi::RTVIMessage::client_ready());
        }
    }
}

void DailyBotsTransport::on_participant_left(
        const nlohmann::json& participant,
        const std::string& reason
) {
    if (_options.callbacks) {
        _options.callbacks->on_bot_disconnected(participant, reason);
    }
}

void DailyBotsTransport::on_rtvi_message(const nlohmann::json& message) {
    if (_options.callbacks) {
        _options.callbacks->on_message(message);
    }

    auto type = message["type"].get<std::string>();
    switch (hash(type.c_str())) {
    case hash("action-response"):
        on_rtvi_action_response(message);
        break;
    case hash("bot-ready"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_ready();
        }
        break;
    case hash("bot-started-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_started_speaking(_bot_participant);
        }
        break;
    case hash("bot-stopped-speaking"):
        if (_options.callbacks) {
            _options.callbacks->on_bot_stopped_speaking(_bot_participant);
        }
        break;
    case hash("tts-text"): {
        auto bot_data = rtvi::BotTTSTextData {
                .text = message["data"]["text"].get<std::string>()
        };
        if (_options.callbacks) {
            _options.callbacks->on_bot_tts_text(bot_data);
        }
        break;
    }
    case hash("bot-tts-text"): {
        auto bot_data = rtvi::BotTTSTextData {
                .text = message["data"]["text"].get<std::string>()
        };
        if (_options.callbacks) {
            _options.callbacks->on_bot_tts_text(bot_data);
        }
        break;
    }
    case hash("bot-llm-text"): {
        auto bot_data = rtvi::BotLLMTextData {
                .text = message["data"]["text"].get<std::string>()
        };
        if (_options.callbacks) {
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
        auto bot_data = rtvi::UserTranscriptData {
                .text = message["data"]["text"].get<std::string>(),
                .final = message["data"]["final"].get<bool>(),
                .timestamp = message["data"]["timestamp"].get<std::string>(),
                .user_id = message["data"]["user_id"].get<std::string>()
        };
        if (_options.callbacks) {
            _options.callbacks->on_user_transcript(bot_data);
        }
        break;
    }
    }
}

void DailyBotsTransport::on_rtvi_action_response(const nlohmann::json& response
) {
    std::unique_lock<std::mutex> lock(_actions_mutex);
    std::string action_id = response["id"].get<std::string>();
    rtvi::RTVIActionCallback action_callback = nullptr;
    if (_action_callbacks.find(action_id) != _action_callbacks.end()) {
        action_callback = _action_callbacks[action_id];
        _action_callbacks.erase(action_id);
    }
    lock.unlock();

    if (action_callback) {
        action_callback(response["data"]);
    }
}
