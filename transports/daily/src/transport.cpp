//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/daily/transport.h"

#include "audio_buffer.h"
#include "context.h"
#include "session.h"

#include <pipecat/errors.h>

extern "C" {
#include "daily_core.h"
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

// How long to wait for daily-core requests, like joining a room.
const std::chrono::seconds REQUEST_TIMEOUT {30};

// The ID of the renderer that receives the bot's audio.
const uint64_t BOT_AUDIO_RENDERER = 1;

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
    //
    // Connection
    //

    explicit Impl(DailyTransportOptions options)
        : _options(options),
          // Keep up to a second of bot audio the app hasn't read.
          _bot_audio(
                  options.bot_audio_channels,
                  options.bot_audio_sample_rate
          ) {}

    ~Impl() {
        disconnect();
        if (_audio_track != nullptr) {
            daily_core_context_destroy_custom_audio_track(_audio_track);
            daily_core_context_track_release(_audio_track);
        }
        // The track has its own reference to the source. This releases ours,
        // which also stops the source's sender thread.
        if (_audio_source != nullptr) {
            daily_core_context_track_release(_audio_source);
        }
    }

    void initialize(TransportObserver* observer) {
        if (_initialized) {
            return;
        }

        _observer = observer;
        _context = std::make_unique<daily::ContextRef>();

        // daily-core's virtual microphone and speaker are one per process, so
        // each transport sends user audio with its own track and receives the
        // bot's audio with a renderer. The source doesn't add silence: the
        // app sends audio continuously, like a microphone does.
        _audio_source = daily_core_context_create_custom_audio_source();
        _audio_track = const_cast<DailyAudioTrack*>(
                daily_core_context_create_custom_audio_track(_audio_source)
        );
        const char* track_id =
                daily_core_context_custom_audio_track_id(_audio_track);
        _audio_track_id = track_id;
        daily_core_string_free(track_id);

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
        delegate.fns.on_audio_data = on_audio_data;
        daily_core_call_client_set_delegate(client, delegate);

        auto session = std::make_shared<daily::Session>(
                _observer,
                [this](std::string message) {
                    send_app_message(std::move(message));
                },
                [this](const std::string& bot_id) { capture_bot_audio(bot_id); }
        );
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _client = client;
            _session = session;
        }
        _bot_audio.open();

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

        // Send user audio from our track, and no video.
        json microphone = {
                {"isEnabled", true},
                {"settings", {{"customTrack", {{"id", _audio_track_id}}}}},
        };
        json inputs = {{"camera", false}, {"microphone", microphone}};
        std::string settings = json {{"inputs", inputs}}.dump();
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
        _bot_audio.close();

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

    //
    // Messages to the bot
    //

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

    //
    // Audio
    //

    int32_t send_user_audio(const int16_t* frames, size_t num_frames) {
        if (!_connected) {
            return 0;
        }
        // Doesn't block, so it can be called from an audio callback.
        daily_core_context_custom_audio_source_write_frames(
                _audio_source,
                frames,
                16,
                static_cast<int32_t>(_options.user_audio_sample_rate),
                _options.user_audio_channels,
                num_frames
        );
        return static_cast<int32_t>(num_frames);
    }

    int32_t read_bot_audio(int16_t* frames, size_t num_frames) {
        return static_cast<int32_t>(_bot_audio.read(frames, num_frames));
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
        uint64_t id;
        {
            std::lock_guard<std::mutex> lock(_requests_mutex);
            id = _next_request_id++;
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

    // Receives the bot's audio with our renderer, without waiting.
    void capture_bot_audio(const std::string& bot_id) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_client == nullptr) {
            return;
        }
        request_async(
                [&](uint64_t id) {
                    daily_core_call_client_set_participant_audio_renderer(
                            _client,
                            id,
                            BOT_AUDIO_RENDERER,
                            bot_id.c_str(),
                            "microphone",
                            _options.bot_audio_sample_rate
                    );
                },
                [this](const RequestResult& result) {
                    if (result) {
                        _observer->on_transport_error(
                                "Unable to receive the bot's audio: " + *result,
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
            _bot_audio.close();
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

    static void on_audio_data(
            DailyRawCallClientDelegate* delegate,
            uint64_t renderer_id,
            const char* /* peer_id */,
            const DailyAudioData* audio
    ) {
        if (renderer_id != BOT_AUDIO_RENDERER || audio->bits_per_sample != 16) {
            return;
        }
        static_cast<Impl*>(delegate)->_bot_audio.write(
                reinterpret_cast<const int16_t*>(audio->audio_frames),
                audio->num_audio_frames,
                audio->num_channels
        );
    }

    DailyTransportOptions _options;

    // Set up once, by initialize().
    TransportObserver* _observer = nullptr;
    bool _initialized = false;
    // Keeps daily-core running while the transport exists.
    std::unique_ptr<daily::ContextRef> _context;
    // Sends the user's audio.
    DailyAudioSource* _audio_source = nullptr;
    DailyAudioTrack* _audio_track = nullptr;
    std::string _audio_track_id;

    // The bot's audio, until the app reads it.
    daily::AudioBuffer _bot_audio;
    // Whether the transport is in a call. Atomic, since the audio methods
    // read it on audio threads.
    std::atomic<bool> _connected {false};

    // Guards the call client and its session.
    std::mutex _mutex;
    DailyRawCallClient* _client = nullptr;
    std::shared_ptr<daily::Session> _session;

    // Requests waiting for their results, by ID.
    std::mutex _requests_mutex;
    uint64_t _next_request_id = 0;
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
