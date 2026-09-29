//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/daily/transport.h"

#include "session.h"

#include <pipecat/errors.h>

extern "C" {
#include "daily_core.h"
#include "daily_core_version.h"
}

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

using nlohmann::json;

namespace pipecat {

namespace {

// NOTE: Do not modify. This is how Daily recognizes a known client library.
const char* DAILY_LIBRARY = "daily-core-sdk";

// How long to wait for daily-core requests, like joining a room.
const std::chrono::seconds REQUEST_TIMEOUT {30};

// Only one daily-core context can exist at a time.
std::atomic<bool> context_in_use {false};

// Result of a daily-core request: empty on success, the error otherwise.
using RequestResult = std::optional<std::string>;

// Called with the result of a daily-core request, on daily-core's thread.
using RequestDone = std::function<void(const RequestResult& result)>;

bool has_string(const json& j, const char* key, const char* value) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() && *it == value;
}

}  // namespace

class DailyTransport::Impl {
   public:
    explicit Impl(DailyTransportOptions options) : _options(options) {}

    ~Impl() {
        disconnect();
        if (_initialized) {
            daily_core_context_destroy();
            context_in_use = false;
        }
    }

    void initialize(TransportObserver* observer) {
        if (_initialized) {
            return;
        }
        if (context_in_use.exchange(true)) {
            throw TransportStartError(
                    "Only one DailyTransport can be initialized at a time"
            );
        }

        _observer = observer;

        daily_core_set_log_level(DailyLogLevel_Off);

        _device_manager = daily_core_context_create_device_manager();

        DailyContextDelegate driver {};

        DailyWebRtcContextDelegate webrtc {};
        webrtc.ptr = this;
        webrtc.fns.get_user_media = get_user_media;
        webrtc.fns.get_enumerated_devices = get_enumerated_devices;
        webrtc.fns.create_audio_device_module = create_audio_device_module;
        webrtc.fns.get_audio_device = get_audio_device;
        webrtc.fns.set_audio_device = set_audio_device;

        DailyAboutClient about {};
        about.library = DAILY_LIBRARY;
        about.version = DAILY_CORE_VERSION;

        daily_core_context_create(driver, webrtc, about);

        _speaker = daily_core_context_create_virtual_speaker_device(
                _device_manager,
                "speaker",
                _options.bot_audio_sample_rate,
                _options.bot_audio_channels,
                false
        );
        daily_core_context_select_speaker_device(_device_manager, "speaker");

        _microphone = daily_core_context_create_virtual_microphone_device(
                _device_manager,
                "mic",
                _options.user_audio_sample_rate,
                _options.user_audio_channels,
                true
        );

        _initialized = true;
    }

    void connect(const json& params) {
        daily::ConnectionParams connection =
                daily::parse_connection_params(params);
        if (!_initialized) {
            throw TransportStartError("DailyTransport is not initialized");
        }

        // Clean up after a session that ended on its own.
        disconnect();

        DailyRawCallClient* client = daily_core_call_client_create();

        DailyCallClientDelegate delegate {};
        delegate.ptr = this;
        delegate.fns.on_event = on_event;
        daily_core_call_client_set_delegate(client, delegate);

        auto session = std::make_shared<daily::Session>(
                _observer,
                [this](std::string message) {
                    send_app_message(std::move(message));
                }
        );
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _client = client;
            _session = session;
        }

        // Only receive the bot's audio.
        std::string profiles = json {
                {"base",
                 {{"camera", "unsubscribed"}, {"microphone", "subscribed"}}}
        }.dump();
        RequestResult result = request([&](uint64_t id) {
            daily_core_call_client_update_subscription_profiles(
                    client, id, profiles.c_str()
            );
        });
        if (result) {
            disconnect();
            throw TransportStartError("Unable to set up Daily: " + *result);
        }

        // Send user audio from the virtual microphone, and no video.
        std::string settings =
                json {{"inputs",
                       {{"camera", false},
                        {"microphone",
                         {{"isEnabled", true},
                          {"settings",
                           {{"deviceId", "mic"},
                            {"customConstraints",
                             {{"echoCancellation", {{"exact", true}}}}}}}}}}}
                }.dump();
        // Before joining, so a call that ends right away stays ended.
        _connected = true;
        result = request([&](uint64_t id) {
            daily_core_call_client_join(
                    client,
                    id,
                    connection.url.c_str(),
                    connection.token.empty() ? nullptr
                                             : connection.token.c_str(),
                    settings.c_str()
            );
        });
        if (result) {
            disconnect();
            throw TransportStartError(
                    "Unable to join the Daily room: " + *result
            );
        }
    }

    void disconnect() {
        DailyRawCallClient* client;
        std::shared_ptr<daily::Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            client = _client;
            session = _session;
        }
        if (client == nullptr) {
            return;
        }

        session->set_leaving();
        _connected = false;

        // Nothing to leave if the call already ended on its own. daily-core
        // runs requests in order, so messages sent before are sent first.
        if (!session->left()) {
            request([&](uint64_t id) {
                daily_core_call_client_leave(client, id);
            });
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _client = nullptr;
            _session.reset();
        }
        daily_core_call_client_destroy(client);

        // No more results arrive once the call client is destroyed.
        std::lock_guard<std::mutex> lock(_requests_mutex);
        _requests.clear();
    }

    void send_ready_message(const rtvi::Message& message) {
        std::shared_ptr<daily::Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            session = _session;
        }
        if (session) {
            session->send_ready_message(json(message).dump());
        }
    }

    void send_message(const rtvi::Message& message) {
        if (_connected) {
            send_app_message(json(message).dump());
        }
    }

    int32_t send_user_audio(const int16_t* frames, size_t num_frames) {
        if (!_connected) {
            return 0;
        }
        return daily_core_context_virtual_microphone_device_write_frames(
                _microphone, frames, num_frames, _request_id++, nullptr, nullptr
        );
    }

    int32_t read_bot_audio(int16_t* frames, size_t num_frames) {
        if (!_connected) {
            return 0;
        }
        return daily_core_context_virtual_speaker_device_read_frames(
                _speaker, frames, num_frames, _request_id++, nullptr, nullptr
        );
    }

   private:
    //
    // daily-core requests. They don't block: daily-core queues them, runs
    // them in order and reports their results as events, on its own thread.
    // So any thread can make them, but waiting for one on daily-core's thread
    // would never end.
    //

    // Makes a daily-core request with a new ID, and calls `done` with its
    // result. Returns the ID.
    template<typename F>
    uint64_t request_async(F make_request, RequestDone done) {
        uint64_t id = _request_id++;
        {
            std::lock_guard<std::mutex> lock(_requests_mutex);
            _requests.emplace(id, std::move(done));
        }
        make_request(id);
        return id;
    }

    // Makes a daily-core request and waits until it completes. Never call it
    // on daily-core's thread.
    template<typename F>
    RequestResult request(F make_request) {
        auto promise = std::make_shared<std::promise<RequestResult>>();
        std::future<RequestResult> future = promise->get_future();
        uint64_t id = request_async(
                make_request, [promise](const RequestResult& result) {
                    promise->set_value(result);
                }
        );

        if (future.wait_for(REQUEST_TIMEOUT) != std::future_status::ready) {
            std::lock_guard<std::mutex> lock(_requests_mutex);
            _requests.erase(id);
            return "request timed out";
        }
        return future.get();
    }

    void complete_request(const json& event) {
        auto request_id = event.find("requestId");
        if (request_id == event.end() || !request_id->is_object()) {
            return;
        }
        auto id = request_id->find("id");
        if (id == request_id->end() || !id->is_number_unsigned()) {
            return;
        }

        RequestResult result;
        auto error = event.find("requestError");
        if (error != event.end() && !error->is_null()) {
            auto message = error->find("msg");
            result = message != error->end() && message->is_string()
                             ? message->get<std::string>()
                             : error->dump();
        }

        RequestDone done;
        {
            std::lock_guard<std::mutex> lock(_requests_mutex);
            auto it = _requests.find(id->get<uint64_t>());
            if (it == _requests.end()) {
                return;
            }
            done = std::move(it->second);
            _requests.erase(it);
        }
        done(result);
    }

    // Sends an app message to the bot, without waiting.
    void send_app_message(std::string message) {
        // Locked, so disconnect() can't destroy the call client meanwhile.
        std::lock_guard<std::mutex> lock(_mutex);
        if (_client == nullptr) {
            return;
        }
        request_async(
                [&](uint64_t id) {
                    daily_core_call_client_send_app_message(
                            _client, id, message.c_str(), nullptr
                    );
                },
                [this](const RequestResult& result) {
                    if (result) {
                        _observer->on_transport_error(
                                "Unable to send a message to the bot: " +
                                        *result,
                                false
                        );
                    }
                }
        );
    }


    //
    // daily-core callbacks. Events arrive on daily-core's thread.
    //

    void handle_event(const char* event_json) {
        json event = json::parse(event_json, nullptr, false);
        if (!event.is_object()) {
            return;
        }

        if (has_string(event, "action", "request-completed")) {
            complete_request(event);
            return;
        }
        if (has_string(event, "action", "call-state-updated") &&
            has_string(event, "state", "left")) {
            _connected = false;
        }

        std::shared_ptr<daily::Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            session = _session;
        }
        if (session) {
            session->handle_event(event);
        }
    }

    static void on_event(
            DailyRawCallClientDelegate* delegate,
            const char* event_json,
            intptr_t /* json_len */
    ) {
        static_cast<Impl*>(delegate)->handle_event(event_json);
    }

    static WebrtcAudioDeviceModule* create_audio_device_module(
            DailyRawWebRtcContextDelegate* delegate,
            WebrtcTaskQueueFactory* task_queue_factory
    ) {
        return daily_core_context_create_audio_device_module(
                static_cast<Impl*>(delegate)->_device_manager,
                task_queue_factory
        );
    }

    static void* get_user_media(
            DailyRawWebRtcContextDelegate* delegate,
            WebrtcPeerConnectionFactory* peer_connection_factory,
            WebrtcThread* signaling_thread,
            WebrtcThread* worker_thread,
            WebrtcThread* network_thread,
            const char* constraints
    ) {
        return daily_core_context_device_manager_get_user_media(
                static_cast<Impl*>(delegate)->_device_manager,
                peer_connection_factory,
                signaling_thread,
                worker_thread,
                network_thread,
                constraints
        );
    }

    static char* get_enumerated_devices(DailyRawWebRtcContextDelegate* delegate
    ) {
        return daily_core_context_device_manager_enumerated_devices(
                static_cast<Impl*>(delegate)->_device_manager
        );
    }

    static const char* get_audio_device(DailyRawWebRtcContextDelegate*) {
        return "";
    }

    static void set_audio_device(DailyRawWebRtcContextDelegate*, const char*) {}

    DailyTransportOptions _options;
    TransportObserver* _observer = nullptr;
    bool _initialized = false;

    DailyDeviceManager* _device_manager = nullptr;
    DailyVirtualSpeakerDevice* _speaker = nullptr;
    DailyVirtualMicrophoneDevice* _microphone = nullptr;

    // Guards the call client and its session.
    std::mutex _mutex;
    DailyRawCallClient* _client = nullptr;
    std::shared_ptr<daily::Session> _session;
    std::atomic<bool> _connected {false};

    std::atomic<uint64_t> _request_id {0};
    // Requests waiting for their results, by ID.
    std::mutex _requests_mutex;
    std::map<uint64_t, RequestDone> _requests;
};

DailyTransport::DailyTransport(DailyTransportOptions options)
    : _impl(std::make_unique<Impl>(options)) {}

DailyTransport::~DailyTransport() = default;

void DailyTransport::initialize(TransportObserver* observer) {
    _impl->initialize(observer);
}

void DailyTransport::connect(const json& params) {
    _impl->connect(params);
}

void DailyTransport::disconnect() {
    _impl->disconnect();
}

void DailyTransport::send_ready_message(const rtvi::Message& message) {
    _impl->send_ready_message(message);
}

void DailyTransport::send_message(const rtvi::Message& message) {
    _impl->send_message(message);
}

int32_t
DailyTransport::send_user_audio(const int16_t* frames, size_t num_frames) {
    return _impl->send_user_audio(frames, num_frames);
}

int32_t DailyTransport::read_bot_audio(int16_t* frames, size_t num_frames) {
    return _impl->read_bot_audio(frames, num_frames);
}

}  // namespace pipecat
