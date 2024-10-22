//
// Copyright (c) 2024, Daily
//

#include "daily_transport.h"

using namespace rtvi;

// NOTE: Do not modify. This is a way for the server to recognize a known
// client library.
static DailyAboutClient ABOUT_CLIENT = {
        .library = "daily-core-sdk",
        .version = "0.11.0"
};

static DailyTransportParams DEFAULT_TRANSPORT_PARAMS = {
        .user_audio_sample_rate = 16000,
        .user_audio_channels = 1,
        .bot_audio_sample_rate = 16000,
        .bot_audio_channels = 1,
};

constexpr unsigned int hash(const char* s, int off = 0) {
    return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
}

static WebrtcAudioDeviceModule* create_audio_device_module_cb(
        DailyRawWebRtcContextDelegate* delegate,
        WebrtcTaskQueueFactory* task_queue_factory
) {
    auto transport = static_cast<DailyTransport*>(delegate);

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
    auto transport = static_cast<DailyTransport*>(delegate);

    return transport->get_user_media(
            peer_connection_factory,
            webrtc_signaling_thread,
            webrtc_worker_thread,
            webrtc_network_thread,
            constraints
    );
}

static char* enumerate_devices_cb(DailyRawWebRtcContextDelegate* delegate) {
    auto transport = static_cast<DailyTransport*>(delegate);

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
    auto transport = static_cast<DailyTransport*>(delegate);

    transport->on_event(event);
}

DailyTransport::DailyTransport(
        const RTVIClientOptions& options,
        RTVITransportMessageObserver* message_observer
)
    : DailyTransport(options, DEFAULT_TRANSPORT_PARAMS, message_observer) {}

DailyTransport::DailyTransport(
        const RTVIClientOptions& options,
        const DailyTransportParams& params,
        RTVITransportMessageObserver* message_observer
)
    : _initialized(false),
      _connected(false),
      _options(options),
      _params(params),
      _message_observer(message_observer),
      _client(nullptr),
      _request_id(0) {}

DailyTransport::~DailyTransport() {
    disconnect();
}

void DailyTransport::initialize() {
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

    _speaker = daily_core_context_create_virtual_speaker_device(
            _device_manager,
            "speaker",
            _params.bot_audio_sample_rate,
            _params.bot_audio_channels,
            false
    );
    daily_core_context_select_speaker_device(_device_manager, "speaker");

    _microphone = daily_core_context_create_virtual_microphone_device(
            _device_manager,
            "mic",
            _params.user_audio_sample_rate,
            _params.user_audio_channels,
            true
    );

    _initialized = true;
}

void DailyTransport::connect(const nlohmann::json& info) {
    if (!_initialized) {
        throw RTVIException("transport is not initialized");
    }
    if (_connected) {
        return;
    }

    if (!info.is_object()) {
        throw RTVIException("invalid connection info: not an object");
    }

    if (!info.contains("room_url") || !info.contains("token")) {
        throw RTVIException(
                "invalid connection info: missing `room_url` or `token`"
        );
    }

    // Cleanup bot participant.
    _bot_participant = nullptr;

    std::string room_url = info["room_url"].get<std::string>();
    std::string token = info["token"].get<std::string>();

    _client = daily_core_call_client_create();

    DailyCallClientDelegate delegate = {
            .ptr = this, .fns = {.on_event = on_event_cb}
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
    _msg_thread = std::thread(&DailyTransport::send_message_thread, this);

    _connected = true;

    if (_options.callbacks) {
        _options.callbacks->on_connected();
    }
}

void DailyTransport::disconnect() {
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

void DailyTransport::send_message(const nlohmann::json& message) {
    if (!_connected) {
        return;
    }

    _msg_queue.push(message);
}

int32_t
DailyTransport::send_user_audio(const int16_t* frames, size_t num_frames) {
    if (!_connected) {
        return 0;
    }

    return daily_core_context_virtual_microphone_device_write_frames(
            _microphone, frames, num_frames, _request_id++, nullptr, nullptr
    );
}

int32_t DailyTransport::read_bot_audio(int16_t* frames, size_t num_frames) {
    if (!_connected) {
        return 0;
    }

    return daily_core_context_virtual_speaker_device_read_frames(
            _speaker, frames, num_frames, _request_id++, nullptr, nullptr
    );
}

// Public but internal

WebrtcAudioDeviceModule* DailyTransport::create_audio_device_module(
        WebrtcTaskQueueFactory* task_queue_factory
) {
    return daily_core_context_create_audio_device_module(
            _device_manager, task_queue_factory
    );
}

char* DailyTransport::enumerate_devices() {
    return daily_core_context_device_manager_enumerated_devices(_device_manager
    );
}

void* DailyTransport::get_user_media(
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

void DailyTransport::on_event(const nlohmann::json& event) {
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
            if (label == "rtvi-ai" && _message_observer) {
                _message_observer->on_transport_message(event["msgData"]);
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

// Private

uint64_t DailyTransport::add_completion(std::promise<void> completion) {
    std::lock_guard<std::mutex> lock(_completions_mutex);

    uint64_t request_id = _request_id++;
    _completions[request_id] = std::move(completion);

    return request_id;
}

void DailyTransport::resolve_completion(uint64_t request_id) {
    std::lock_guard<std::mutex> lock(_completions_mutex);
    _completions[request_id].set_value();
    _completions.erase(request_id);
}

void DailyTransport::send_message_thread() {
    bool running = true;
    while (running) {
        std::optional<nlohmann::json> message = _msg_queue.blocking_pop();
        if (message.has_value()) {
            std::string data = (*message).dump();

            std::promise<void> msg_promise;
            std::future<void> msg_future = msg_promise.get_future();
            uint64_t request_id = add_completion(std::move(msg_promise));
            daily_core_call_client_send_app_message(
                    _client, request_id, data.c_str(), nullptr
            );
            msg_future.get();
        } else {
            running = false;
        }
    }
}

void DailyTransport::on_participant_joined(const nlohmann::json& participant) {
    std::string participant_id = participant["id"].get<std::string>();

    // We assume the first remote participant is a bot.
    if (_bot_participant.is_null()) {
        _bot_participant = participant;

        if (_options.callbacks) {
            _options.callbacks->on_bot_connected(participant);
        }
    }
}

void DailyTransport::on_participant_updated(const nlohmann::json& participant) {
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
            send_message(RTVIMessage::client_ready());
        }
    }
}

void DailyTransport::on_participant_left(
        const nlohmann::json& participant,
        const std::string& reason
) {
    if (_options.callbacks) {
        _options.callbacks->on_bot_disconnected(participant, reason);
    }
}
