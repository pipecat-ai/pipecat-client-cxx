//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/websocket/transport.h"

#include "frames.h"

#include <pipecat/errors.h>

#include <rtc/rtc.hpp>

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

using nlohmann::json;

namespace pipecat {

namespace {

// Pipecat's WebSocket server (Python's websockets) accepts messages of up to
// 1 MiB, frames included.
const size_t MAX_MESSAGE_SIZE = 1024 * 1024;
// What a message's frame adds to its JSON, at most.
const size_t FRAME_OVERHEAD = 16;

const std::chrono::seconds CONNECT_TIMEOUT {10};
const std::chrono::seconds CLOSE_TIMEOUT {5};

// Percent-encodes everything but unreserved characters (RFC 3986).
std::string url_encode(const std::string& value) {
    static const char* digits = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c: value) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(digits[c >> 4]);
            encoded.push_back(digits[c & 0xf]);
        }
    }
    return encoded;
}

std::string string_param(const json& params, const char* key) {
    auto it = params.find(key);
    return it != params.end() && it->is_string() ? it->get<std::string>() : "";
}

// The URL to connect to: `wsUrl` (or `ws_url`), with the token, if any.
std::string connection_url(const json& params) {
    if (!params.is_object()) {
        throw InvalidTransportParamsError(
                "WebSocket connection params must be a JSON object"
        );
    }

    std::string url = string_param(params, "wsUrl");
    if (url.empty()) {
        url = string_param(params, "ws_url");
    }
    if (url.empty()) {
        throw InvalidTransportParamsError(
                "WebSocket connection params need the bot's URL, in `wsUrl`"
        );
    }

    std::string token = string_param(params, "token");
    if (!token.empty()) {
        url += url.find('?') == std::string::npos ? '?' : '&';
        url += "token=" + url_encode(token);
    }
    return url;
}

}  // namespace

class WebSocketTransport::Impl {
   public:
    //
    // Connection
    //

    explicit Impl(WebSocketTransportOptions options) : _options(options) {}

    ~Impl() { disconnect(); }

    void initialize(TransportObserver* observer) { _observer = observer; }

    void connect(const json& params) {
        std::string url = connection_url(params);

        // Clean up after a connection that ended on its own.
        disconnect();

        rtc::WebSocket::Configuration config;
        config.maxMessageSize = MAX_MESSAGE_SIZE;
        config.connectionTimeout = CONNECT_TIMEOUT;
        auto ws = std::make_shared<rtc::WebSocket>(config);
        ws->onOpen([this] { handle_open(); });
        ws->onError([this](std::string error) { handle_error(error); });
        ws->onClosed([this] { handle_closed(); });
        ws->onMessage(
                [this](rtc::binary data) { handle_frame(data); },
                [](rtc::string) {}
        );

        std::unique_lock<std::mutex> lock(_mutex);
        _ws = ws;
        _state = State::Opening;
        _open_error.reset();
        lock.unlock();

        try {
            ws->open(url);
        } catch (const std::invalid_argument& e) {
            // It never opened, so no callback runs.
            lock.lock();
            _ws.reset();
            _state = State::Closed;
            throw InvalidTransportParamsError(e.what());
        }

        lock.lock();
        // libdatachannel times out first, so this is only a safety net.
        bool done = _cv.wait_for(lock, CONNECT_TIMEOUT * 2, [this] {
            return _state != State::Opening;
        });
        if (_state != State::Open) {
            std::string error =
                    done ? _open_error.value_or("closed") : "timed out";
            lock.unlock();
            disconnect();
            throw TransportStartError(
                    "Unable to connect to the bot's WebSocket: " + error
            );
        }
        _connected = true;
    }

    void disconnect() {
        std::unique_lock<std::mutex> lock(_mutex);
        std::shared_ptr<rtc::WebSocket> ws = std::move(_ws);
        if (!ws) {
            return;
        }
        _connected = false;
        if (_state != State::Closed) {
            // Closing it ourselves isn't an unexpected disconnection.
            _state = State::Closing;
            lock.unlock();
            ws->close();
            lock.lock();
            // libdatachannel can't reset callbacks while it may still call
            // them, so wait for the last one.
            _cv.wait_for(lock, CLOSE_TIMEOUT, [this] {
                return _state == State::Closed;
            });
            _state = State::Closed;
        }
        lock.unlock();

        // Waits for callbacks in progress, so none runs after this.
        ws->resetCallbacks();
    }

    //
    // Messages to the bot
    //

    void send_ready_message(const rtvi::Message& message) {
        send_message(message);
    }

    void send_message(const rtvi::Message& message) {
        if (_connected) {
            send_frame(websocket::encode_message(json(message).dump()));
        }
    }

    size_t max_message_size() const {
        return MAX_MESSAGE_SIZE - FRAME_OVERHEAD;
    }

    //
    // Audio
    //

    int32_t send_user_audio(const int16_t* frames, size_t num_frames) {
        if (!_connected) {
            return 0;
        }
        send_frame(
                websocket::encode_audio(
                        frames,
                        num_frames,
                        _options.user_audio_sample_rate,
                        _options.user_audio_channels
                )
        );
        return static_cast<int32_t>(num_frames);
    }

    int32_t read_bot_audio(int16_t* /* frames */, size_t /* num_frames */) {
        return 0;
    }

   private:
    // Where the WebSocket is.
    enum class State { Closed, Opening, Open, Closing };

    void send_frame(const std::string& frame) {
        std::shared_ptr<rtc::WebSocket> ws;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            ws = _ws;
        }
        if (!ws) {
            return;
        }
        try {
            ws->send(
                    reinterpret_cast<const std::byte*>(frame.data()),
                    frame.size()
            );
        } catch (const std::exception&) {
            // It's closing, which is reported on its own.
        }
    }

    //
    // WebSocket callbacks. They run on libdatachannel's threads.
    //

    void handle_open() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state == State::Opening) {
            _state = State::Open;
            _cv.notify_all();
        }
    }

    void handle_error(const std::string& error) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            // When opening, connect() reports it once the WebSocket closes.
            if (_state == State::Opening) {
                _open_error = error;
                return;
            }
            if (_state != State::Open) {
                return;
            }
        }
        _observer->on_transport_error("WebSocket error: " + error, false);
    }

    void handle_closed() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            State state = _state;
            _state = State::Closed;
            _cv.notify_all();
            // When opening, connect() reports it. When closing, it was us.
            if (state != State::Open) {
                return;
            }
        }
        _connected = false;
        _observer->on_transport_disconnected();
    }

    void handle_frame(const rtc::binary& data) {
        websocket::Frame frame;
        try {
            frame = websocket::decode(data.data(), data.size());
        } catch (const std::exception& e) {
            _observer->on_transport_error(
                    std::string("Invalid frame from the bot: ") + e.what(),
                    false
            );
            return;
        }

        if (auto* message = std::get_if<websocket::MessageFrame>(&frame)) {
            json parsed = json::parse(message->data, nullptr, false);
            if (parsed.is_discarded()) {
                _observer->on_transport_error(
                        "Invalid message from the bot: not JSON", false
                );
                return;
            }
            _observer->on_transport_message(parsed);
        }
    }

    WebSocketTransportOptions _options;
    TransportObserver* _observer = nullptr;

    // Whether the WebSocket is open. Atomic, since the audio methods read it
    // on audio threads.
    std::atomic<bool> _connected {false};

    // Guards the WebSocket and its state. _cv wakes up connect() and
    // disconnect() when the state changes.
    std::mutex _mutex;
    std::condition_variable _cv;
    std::shared_ptr<rtc::WebSocket> _ws;
    State _state = State::Closed;
    // Why the WebSocket failed to open.
    std::optional<std::string> _open_error;
};

WebSocketTransport::WebSocketTransport(WebSocketTransportOptions options)
    : _impl(std::make_unique<Impl>(options)) {}

WebSocketTransport::~WebSocketTransport() = default;

void WebSocketTransport::initialize(TransportObserver* observer) {
    _impl->initialize(observer);
}

void WebSocketTransport::connect(const json& params) {
    _impl->connect(params);
}

void WebSocketTransport::disconnect() {
    _impl->disconnect();
}

void WebSocketTransport::send_ready_message(const rtvi::Message& message) {
    _impl->send_ready_message(message);
}

void WebSocketTransport::send_message(const rtvi::Message& message) {
    _impl->send_message(message);
}

size_t WebSocketTransport::max_message_size() const {
    return _impl->max_message_size();
}

int32_t
WebSocketTransport::send_user_audio(const int16_t* frames, size_t num_frames) {
    return _impl->send_user_audio(frames, num_frames);
}

int32_t WebSocketTransport::read_bot_audio(int16_t* frames, size_t num_frames) {
    return _impl->read_bot_audio(frames, num_frames);
}

}  // namespace pipecat
