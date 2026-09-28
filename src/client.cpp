//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/client.h"

#include "event_loop.h"
#include "http.h"

#include "pipecat/errors.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

using nlohmann::json;

namespace pipecat {

namespace {

// States in which start_bot() and connect() can't be called.
bool is_busy(TransportState state) {
    switch (state) {
    case TransportState::Authenticating:
    case TransportState::Connecting:
    case TransportState::Connected:
    case TransportState::Ready:
    case TransportState::Disconnecting:
        return true;
    default:
        return false;
    }
}

// States disconnect() has to undo.
bool is_started(TransportState state) {
    switch (state) {
    case TransportState::Authenticating:
    case TransportState::Authenticated:
    case TransportState::Connecting:
    case TransportState::Connected:
    case TransportState::Ready:
        return true;
    default:
        return false;
    }
}

bool is_connected(TransportState state) {
    return state == TransportState::Connected || state == TransportState::Ready;
}

// Error message for a failed start endpoint response. Like client-js, use
// the `info` or `detail` fields of a JSON body if there are any.
std::string start_bot_error_message(const HttpResponse& response) {
    json body = json::parse(response.body, nullptr, false);
    if (body.is_object()) {
        for (const char* key: {"info", "detail"}) {
            auto it = body.find(key);
            if (it != body.end() && !it->is_null()) {
                return it->is_string() ? it->get<std::string>() : it->dump();
            }
        }
    }
    return "Start endpoint returned HTTP " + std::to_string(response.status);
}

}  // namespace

class PipecatClient::Impl : public TransportObserver {
   public:
    explicit Impl(PipecatClientOptions options);

    // Disconnects, runs the callbacks still queued and stops the threads.
    // Called by ~PipecatClient() before the Impl is destroyed, since those
    // callbacks can still call the client.
    void close();

    void initialize();
    json start_bot(const APIRequest& request);
    rtvi::BotReadyData connect(const json& transport_params);
    void disconnect();
    TransportState state() const;
    bool connected() const;
    Transport& transport() { return *_transport; }
    int32_t send_user_audio(const int16_t* frames, size_t num_frames);
    int32_t read_bot_audio(int16_t* frames, size_t num_frames);

    // TransportObserver
    void on_transport_message(const json& message) override;
    void on_bot_connected(const Participant& bot) override;
    void on_bot_disconnected(const Participant& bot) override;
    void on_participant_joined(const Participant& participant) override;
    void on_participant_left(const Participant& participant) override;
    void on_transport_disconnected() override;

   private:
    using Callback = std::function<void(PipecatClientCallbacks&)>;

    // Queues a callback on the event loop. notify_locked() expects _mutex
    // to be held.
    void notify(Callback callback);
    void notify_locked(Callback callback);

    // Queues a callback with no arguments.
    void notify_event(void (PipecatClientCallbacks::*method)());

    // Parses `data` as T here, on the caller's thread, and queues a callback
    // with it.
    template<typename T>
    void notify_data(
            const json& data,
            void (PipecatClientCallbacks::*method)(const T&)
    ) {
        notify([parsed = data.get<T>(), method](PipecatClientCallbacks& c) {
            (c.*method)(parsed);
        });
    }

    void report_error(const std::string& error, bool fatal);

    // Expects _mutex to be held.
    void set_state_locked(TransportState state);
    void check_can_start_locked() const;

    bool is_cancelled(uint64_t session) const;

    // Disconnects, but only if the session is still `session`.
    void disconnect_session(std::optional<uint64_t> session);

    // Disconnects after connect() fails and moves to the Error state, unless
    // the connection was already cancelled. `lock` must hold _mutex, so the
    // caller can check why it failed without letting other events in.
    void fail_connect(uint64_t session, std::unique_lock<std::mutex> lock);

    // Ends the session: disconnects the transport and moves to
    // `end_state`. `lock` must hold _mutex.
    void
    end_session(std::unique_lock<std::mutex> lock, TransportState end_state);

    void handle_message(const rtvi::Message& message);
    void handle_bot_ready(const rtvi::BotReadyData& data);

    PipecatClientCallbacks* _callbacks;
    bool _disconnect_on_bot_disconnect;
    std::chrono::milliseconds _connect_timeout;
    rtvi::AboutClientData _about;

    mutable std::mutex _mutex;
    // Notified whenever the state changes.
    std::condition_variable _cv;
    // Changed with _mutex held, but read without it where only the current
    // state matters, so e.g. the audio methods never wait on audio threads.
    std::atomic<TransportState> _state {TransportState::Disconnected};
    // Incremented by start_bot(), connect() and every disconnection, so work
    // in progress can tell it was cancelled.
    uint64_t _session = 0;
    std::optional<rtvi::BotReadyData> _bot_ready;
    // start_bot() and connect() fail once the destructor runs.
    bool _destroying = false;

    // Serializes transport initialize(), connect() and disconnect() calls.
    std::mutex _transport_mutex;
    bool _initialized = false;

    EventLoop _loop;
    // Destroyed first, while the rest is still alive, since the transport can
    // report events while it shuts down.
    std::unique_ptr<Transport> _transport;
};

PipecatClient::Impl::Impl(PipecatClientOptions options)
    : _callbacks(options.callbacks),
      _disconnect_on_bot_disconnect(options.disconnect_on_bot_disconnect),
      _connect_timeout(options.connect_timeout),
      _about(std::move(options.about)),
      _transport(std::move(options.transport)) {
    if (!_transport) {
        throw PipecatError("PipecatClientOptions::transport is required");
    }
}

void PipecatClient::Impl::close() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _destroying = true;
    }

    try {
        disconnect();
    } catch (...) {
    }

    // Runs the callbacks still queued, e.g. on_disconnected(). Anything
    // queued after this is ignored.
    _loop.stop();
}

void PipecatClient::Impl::initialize() {
    std::lock_guard<std::mutex> transport_lock(_transport_mutex);
    if (_initialized) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_destroying) {
            throw PipecatError("Client is being destroyed");
        }
        set_state_locked(TransportState::Initializing);
    }

    try {
        _transport->initialize(this);
    } catch (...) {
        std::lock_guard<std::mutex> lock(_mutex);
        set_state_locked(TransportState::Error);
        throw;
    }

    _initialized = true;

    std::lock_guard<std::mutex> lock(_mutex);
    set_state_locked(TransportState::Initialized);
}

json PipecatClient::Impl::start_bot(const APIRequest& request) {
    initialize();

    uint64_t session;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        check_can_start_locked();
        session = ++_session;
        set_state_locked(TransportState::Authenticating);
    }

    auto fail = [&](const std::string& error) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (session != _session) {
            return;
        }
        set_state_locked(TransportState::Error);
        rtvi::ErrorData data;
        data.error = error;
        data.fatal = true;
        notify_locked([data](PipecatClientCallbacks& c) { c.on_error(data); });
    };

    json response;
    try {
        HttpResponse http_response =
                http_post(request, [&] { return is_cancelled(session); });
        if (http_response.status < 200 || http_response.status >= 300) {
            throw StartBotError(
                    start_bot_error_message(http_response),
                    static_cast<int>(http_response.status)
            );
        }
        if (!http_response.body.empty()) {
            response = json::parse(http_response.body, nullptr, false);
            if (response.is_discarded()) {
                throw StartBotError(
                        "Start endpoint returned invalid JSON",
                        static_cast<int>(http_response.status)
                );
            }
        }
    } catch (const StartBotError& e) {
        fail(e.what());
        throw;
    } catch (...) {
        // Cancelled by disconnect(), which already updated the state so fail()
        // does nothing, or an unexpected error.
        fail("start_bot() failed");
        throw;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (session != _session) {
        throw PipecatError("start_bot() was cancelled by disconnect()");
    }
    set_state_locked(TransportState::Authenticated);
    notify_locked([response](PipecatClientCallbacks& c) {
        c.on_bot_started(response);
    });
    return response;
}

rtvi::BotReadyData PipecatClient::Impl::connect(const json& transport_params) {
    initialize();

    uint64_t session;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        check_can_start_locked();
        session = ++_session;
        _bot_ready.reset();
        set_state_locked(TransportState::Connecting);
    }

    try {
        std::lock_guard<std::mutex> transport_lock(_transport_mutex);
        _transport->connect(transport_params);
        // If disconnect() ran before we got the transport lock, it had
        // nothing to disconnect yet, so undo the connection here.
        if (is_cancelled(session)) {
            _transport->disconnect();
            throw PipecatError("connect() was cancelled by disconnect()");
        }
    } catch (...) {
        fail_connect(session, std::unique_lock<std::mutex>(_mutex));
        throw;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (session != _session) {
            // disconnect() ran after the transport connected. It
            // disconnects the transport once it gets the transport lock.
            throw PipecatError("connect() was cancelled by disconnect()");
        }
        // The bot might already be ready if it was very fast.
        if (_state == TransportState::Connecting) {
            set_state_locked(TransportState::Connected);
        }
        notify_locked([](PipecatClientCallbacks& c) { c.on_connected(); });
    }

    try {
        _transport->send_ready_message(rtvi::Message::client_ready(_about));
    } catch (...) {
        fail_connect(session, std::unique_lock<std::mutex>(_mutex));
        throw;
    }

    std::unique_lock<std::mutex> lock(_mutex);
    auto done = [&] { return session != _session || _bot_ready.has_value(); };
    bool ready = true;
    if (_connect_timeout.count() > 0) {
        ready = _cv.wait_for(lock, _connect_timeout, done);
    } else {
        _cv.wait(lock, done);
    }

    if (session != _session) {
        throw PipecatError("Disconnected before the bot was ready");
    }
    if (!ready) {
        // Still locked, so a bot-ready arriving now is ignored.
        fail_connect(session, std::move(lock));
        throw ConnectionTimeoutError();
    }
    return *_bot_ready;
}

void PipecatClient::Impl::disconnect() {
    disconnect_session(std::nullopt);
}

void PipecatClient::Impl::disconnect_session(std::optional<uint64_t> session) {
    std::unique_lock<std::mutex> lock(_mutex);
    if (session && *session != _session) {
        return;
    }
    // Another disconnection is in progress, e.g. after the bot left. Wait for
    // it, so the client can connect again once this returns.
    _cv.wait(lock, [this] { return _state != TransportState::Disconnecting; });
    if (is_started(_state)) {
        end_session(std::move(lock), TransportState::Disconnected);
    }
}

void PipecatClient::Impl::fail_connect(
        uint64_t session,
        std::unique_lock<std::mutex> lock
) {
    if (session == _session) {
        end_session(std::move(lock), TransportState::Error);
    }
}

void PipecatClient::Impl::end_session(
        std::unique_lock<std::mutex> lock,
        TransportState end_state
) {
    bool was_connected = is_connected(_state);
    ++_session;
    set_state_locked(TransportState::Disconnecting);
    lock.unlock();

    {
        std::lock_guard<std::mutex> transport_lock(_transport_mutex);
        try {
            _transport->disconnect();
        } catch (const std::exception& e) {
            report_error(
                    std::string("Error disconnecting: ") + e.what(), false
            );
        }
    }

    lock.lock();
    set_state_locked(end_state);
    if (was_connected) {
        notify_locked([](PipecatClientCallbacks& c) { c.on_disconnected(); });
    }
}

TransportState PipecatClient::Impl::state() const {
    return _state;
}

bool PipecatClient::Impl::connected() const {
    return is_connected(_state);
}

int32_t
PipecatClient::Impl::send_user_audio(const int16_t* frames, size_t num_frames) {
    if (!connected()) {
        return 0;
    }
    return _transport->send_user_audio(frames, num_frames);
}

int32_t
PipecatClient::Impl::read_bot_audio(int16_t* frames, size_t num_frames) {
    if (!connected()) {
        return 0;
    }
    return _transport->read_bot_audio(frames, num_frames);
}

//
// TransportObserver. Called on transport threads, so only queue work.
//

void PipecatClient::Impl::on_transport_message(const json& raw) {
    rtvi::Message message;
    try {
        message = raw.get<rtvi::Message>();
        if (message.label != rtvi::MESSAGE_LABEL) {
            return;
        }
        handle_message(message);
    } catch (const std::exception& e) {
        std::string type = message.type.empty() ? "unknown" : message.type;
        report_error("Invalid RTVI message (" + type + "): " + e.what(), false);
    }
}

void PipecatClient::Impl::on_bot_connected(const Participant& bot) {
    notify([bot](PipecatClientCallbacks& c) { c.on_bot_connected(bot); });
}

void PipecatClient::Impl::on_bot_disconnected(const Participant& bot) {
    std::lock_guard<std::mutex> lock(_mutex);
    notify_locked([bot](PipecatClientCallbacks& c) {
        c.on_bot_disconnected(bot);
    });
    if (_disconnect_on_bot_disconnect) {
        // After the callback, and only if nothing reconnected in between.
        uint64_t session = _session;
        _loop.post([this, session] { disconnect_session(session); });
    }
}

void PipecatClient::Impl::on_participant_joined(const Participant& participant
) {
    notify([participant](PipecatClientCallbacks& c) {
        c.on_participant_joined(participant);
    });
}

void PipecatClient::Impl::on_participant_left(const Participant& participant) {
    notify([participant](PipecatClientCallbacks& c) {
        c.on_participant_left(participant);
    });
}

void PipecatClient::Impl::on_transport_disconnected() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!is_started(_state)) {
        return;
    }
    bool was_connected = is_connected(_state);
    ++_session;
    set_state_locked(TransportState::Disconnected);
    if (was_connected) {
        notify_locked([](PipecatClientCallbacks& c) { c.on_disconnected(); });
    }
}

//
// Messages
//

void PipecatClient::Impl::handle_message(const rtvi::Message& message) {
    using rtvi::MessageType;
    using C = PipecatClientCallbacks;

    const json& data = message.data;

    auto type = rtvi::parse_message_type(message.type);
    if (!type) {
        notify([message](C& c) { c.on_unhandled_message(message); });
        return;
    }

    switch (*type) {
    case MessageType::BotReady:
        handle_bot_ready(data.get<rtvi::BotReadyData>());
        break;
    case MessageType::Error:
        notify_data(data, &C::on_error);
        break;
    case MessageType::ErrorResponse:
        notify_data(data, &C::on_message_error);
        break;
    case MessageType::ServerMessage:
        notify([data](C& c) { c.on_server_message(data); });
        break;
    case MessageType::ServerResponse:
        // Answers a client request, and this client doesn't send any.
        break;
    case MessageType::Metrics:
        notify_data(data, &C::on_metrics);
        break;
    case MessageType::UserStartedSpeaking:
        notify_event(&C::on_user_started_speaking);
        break;
    case MessageType::UserStoppedSpeaking:
        notify_event(&C::on_user_stopped_speaking);
        break;
    case MessageType::BotStartedSpeaking:
        notify_event(&C::on_bot_started_speaking);
        break;
    case MessageType::BotStoppedSpeaking:
        notify_event(&C::on_bot_stopped_speaking);
        break;
    case MessageType::UserMuteStarted:
        notify_event(&C::on_user_mute_started);
        break;
    case MessageType::UserMuteStopped:
        notify_event(&C::on_user_mute_stopped);
        break;
    case MessageType::UserTranscription:
        notify_data(data, &C::on_user_transcript);
        break;
    case MessageType::UserLLMText:
        notify_data(data, &C::on_user_llm_text);
        break;
    case MessageType::BotOutput:
        notify_data(data, &C::on_bot_output);
        break;
    case MessageType::BotTranscription:
        // Deprecated and superseded by bot-output.
        break;
    case MessageType::BotLLMText:
        notify_data(data, &C::on_bot_llm_text);
        break;
    case MessageType::BotLLMStarted:
        notify_event(&C::on_bot_llm_started);
        break;
    case MessageType::BotLLMStopped:
        notify_event(&C::on_bot_llm_stopped);
        break;
    case MessageType::BotLLMSearchResponse:
        notify_data(data, &C::on_bot_llm_search_response);
        break;
    case MessageType::BotTTSText:
        notify_data(data, &C::on_bot_tts_text);
        break;
    case MessageType::BotTTSStarted:
        notify_event(&C::on_bot_tts_started);
        break;
    case MessageType::BotTTSStopped:
        notify_event(&C::on_bot_tts_stopped);
        break;
    case MessageType::LLMFunctionCall:
    case MessageType::LLMFunctionCallInProgress:
        // The deprecated llm-function-call parses as in-progress data.
        notify_data(data, &C::on_llm_function_call_in_progress);
        break;
    case MessageType::LLMFunctionCallStarted:
        notify_data(data, &C::on_llm_function_call_started);
        break;
    case MessageType::LLMFunctionCallStopped:
        notify_data(data, &C::on_llm_function_call_stopped);
        break;
    case MessageType::ClientReady:
    case MessageType::DisconnectBot:
    case MessageType::ClientMessage:
    case MessageType::SendText:
    case MessageType::DTMF:
    case MessageType::LLMFunctionCallResult:
        // Client-to-server messages, not expected from the bot.
        notify([message](C& c) { c.on_unhandled_message(message); });
        break;
    }
}

void PipecatClient::Impl::handle_bot_ready(const rtvi::BotReadyData& data) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state == TransportState::Connecting ||
        _state == TransportState::Connected) {
        _bot_ready = data;
        set_state_locked(TransportState::Ready);
        notify_locked([data](PipecatClientCallbacks& c) {
            c.on_bot_ready(data);
        });
    }
}

//
// Helpers
//

void PipecatClient::Impl::notify(Callback callback) {
    std::lock_guard<std::mutex> lock(_mutex);
    notify_locked(std::move(callback));
}

void PipecatClient::Impl::notify_locked(Callback callback) {
    if (_callbacks == nullptr) {
        return;
    }
    _loop.post([callbacks = _callbacks, callback = std::move(callback)] {
        callback(*callbacks);
    });
}

void PipecatClient::Impl::notify_event(void (PipecatClientCallbacks::*method)()
) {
    notify([method](PipecatClientCallbacks& c) { (c.*method)(); });
}

void PipecatClient::Impl::report_error(const std::string& error, bool fatal) {
    rtvi::ErrorData data;
    data.error = error;
    data.fatal = fatal;
    notify([data](PipecatClientCallbacks& c) { c.on_error(data); });
}

void PipecatClient::Impl::set_state_locked(TransportState state) {
    if (_state == state) {
        return;
    }
    _state = state;
    _cv.notify_all();
    notify_locked([state](PipecatClientCallbacks& c) {
        c.on_transport_state_changed(state);
    });
}

void PipecatClient::Impl::check_can_start_locked() const {
    if (_destroying) {
        throw PipecatError("Client is being destroyed");
    }
    if (is_busy(_state)) {
        throw BotAlreadyStartedError();
    }
}

bool PipecatClient::Impl::is_cancelled(uint64_t session) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return session != _session;
}

//
// PipecatClient
//

PipecatClient::PipecatClient(PipecatClientOptions options)
    : _impl(std::make_unique<Impl>(std::move(options))) {}

PipecatClient::~PipecatClient() {
    _impl->close();
}

void PipecatClient::initialize() {
    _impl->initialize();
}

json PipecatClient::start_bot(const APIRequest& request) {
    return _impl->start_bot(request);
}

rtvi::BotReadyData PipecatClient::connect(const json& transport_params) {
    return _impl->connect(transport_params);
}

rtvi::BotReadyData PipecatClient::start_bot_and_connect(
        const APIRequest& request
) {
    return connect(start_bot(request));
}

void PipecatClient::disconnect() {
    _impl->disconnect();
}

TransportState PipecatClient::state() const {
    return _impl->state();
}

bool PipecatClient::connected() const {
    return _impl->connected();
}

Transport& PipecatClient::transport() {
    return _impl->transport();
}

int32_t
PipecatClient::send_user_audio(const int16_t* frames, size_t num_frames) {
    return _impl->send_user_audio(frames, num_frames);
}

int32_t PipecatClient::read_bot_audio(int16_t* frames, size_t num_frames) {
    return _impl->read_bot_audio(frames, num_frames);
}

}  // namespace pipecat
