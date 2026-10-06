//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/smallwebrtc/transport.h"

#include "http.h"
#include "params.h"

#include <pipecat/client.h>
#include <pipecat/errors.h>

#include <rtc/rtc.hpp>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <string>

using nlohmann::json;

namespace pipecat {

namespace {

// How long to wait for our ICE candidates before sending the offer without
// the rest, like client-js's waitForICEGathering.
const std::chrono::seconds GATHERING_TIMEOUT {2};
// The bot's server may only start the bot once the offer arrives.
const std::chrono::seconds OFFER_TIMEOUT {30};
const std::chrono::seconds CONNECT_TIMEOUT {10};
const std::chrono::seconds CLOSE_TIMEOUT {5};

// The bot reads messages from the data channel the client creates.
const char* DATA_CHANNEL_LABEL = "chat";
// Opus, like browsers.
const int OPUS_PAYLOAD_TYPE = 111;

// Messages about the connection itself, from the bot.
const char* SIGNALLING_TYPE = "signalling";
const char* PEER_LEFT_TYPE = "peerLeft";

// Whether `message` is a JSON object of type `type`.
bool is_type(const json& message, const char* type) {
    if (!message.is_object()) {
        return false;
    }
    auto it = message.find("type");
    return it != message.end() && *it == type;
}

}  // namespace

class SmallWebRTCTransport::Impl {
   public:
    //
    // Connection
    //

    ~Impl() { disconnect(); }

    void initialize(TransportObserver* observer) { _observer = observer; }

    void set_start_bot_params(const APIRequest& request) {
        _start_request = request;
    }

    void connect(const json& params) {
        smallwebrtc::ConnectionParams connection =
                smallwebrtc::parse_connection_params(params, _start_request);

        // Clean up after a connection that ended on its own.
        disconnect();

        rtc::Configuration config;
        config.iceServers = connection.ice_servers;
        // The offer is made once everything is added.
        config.disableAutoNegotiation = true;
        auto pc = std::make_shared<rtc::PeerConnection>(config);
        pc->onStateChange([this](rtc::PeerConnection::State state) {
            handle_state(state);
        });
        pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState) {
            std::lock_guard<std::mutex> lock(_mutex);
            _cv.notify_all();
        });

        // The bot expects audio on the first transceiver, and its audio output
        // stalls if nobody takes it. Its audio is dropped for now.
        rtc::Description::Audio audio(
                "0", rtc::Description::Direction::SendRecv
        );
        audio.addOpusCodec(OPUS_PAYLOAD_TYPE);
        auto track = pc->addTrack(audio);

        auto dc = pc->createDataChannel(DATA_CHANNEL_LABEL);
        dc->onOpen([this] { handle_open(); });
        dc->onMessage(
                [](rtc::binary) {},
                [this](rtc::string message) { handle_message(message); }
        );

        std::unique_lock<std::mutex> lock(_mutex);
        _pc = pc;
        _track = track;
        _dc = dc;
        _state = State::Connecting;
        lock.unlock();

        pc->setLocalDescription(rtc::Description::Type::Offer);

        // Send our candidates with the offer.
        lock.lock();
        _cv.wait_for(lock, GATHERING_TIMEOUT, [&] {
            return pc->gatheringState() ==
                           rtc::PeerConnection::GatheringState::Complete ||
                   _state != State::Connecting;
        });
        lock.unlock();

        json answer =
                send_offer(connection, std::string(*pc->localDescription()));
        try {
            pc->setRemoteDescription(
                    rtc::Description(
                            answer["sdp"].get<std::string>(),
                            answer["type"].get<std::string>()
                    )
            );
        } catch (const std::exception& e) {
            fail(std::string("Invalid WebRTC answer from the bot: ") +
                 e.what());
        }

        lock.lock();
        bool done = _cv.wait_for(lock, CONNECT_TIMEOUT, [this] {
            return _state != State::Connecting;
        });
        if (_state != State::Open) {
            lock.unlock();
            fail(done ? "the connection failed" : "timed out");
        }
        // The bot's peer connection ID identifies it, like in client-js.
        auto pc_id = answer.find("pc_id");
        _bot.id = pc_id != answer.end() && pc_id->is_string()
                          ? pc_id->get<std::string>()
                          : "bot";
        _bot.name = "bot";
        Participant bot = _bot;
        lock.unlock();

        _observer->on_bot_connected(bot);
    }

    void disconnect() {
        std::unique_lock<std::mutex> lock(_mutex);
        std::shared_ptr<rtc::PeerConnection> pc = std::move(_pc);
        std::shared_ptr<rtc::Track> track = std::move(_track);
        std::shared_ptr<rtc::DataChannel> dc = std::move(_dc);
        if (!pc) {
            return;
        }
        if (_state != State::Closed) {
            // Closing it ourselves isn't an unexpected disconnection.
            _state = State::Closing;
            lock.unlock();
            pc->close();
            lock.lock();
            // Wait until the bot knows, so it can end the session.
            _cv.wait_for(lock, CLOSE_TIMEOUT, [this] {
                return _state == State::Closed;
            });
            _state = State::Closed;
        }
        lock.unlock();

        // Wait for callbacks in progress, so none runs after this.
        track->resetCallbacks();
        dc->resetCallbacks();
        pc->resetCallbacks();
    }

    //
    // Messages to the bot
    //

    void send_ready_message(const rtvi::Message& message) {
        send_message(message);
    }

    void send_message(const rtvi::Message& message) {
        std::shared_ptr<rtc::DataChannel> dc;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == State::Open) {
                dc = _dc;
            }
        }
        if (!dc) {
            return;
        }
        try {
            dc->send(json(message).dump());
        } catch (const std::exception&) {
            // It's closing, which is reported on its own.
        }
    }

   private:
    // Where the connection is.
    enum class State { Closed, Connecting, Open, Closing };

    // Sends the offer to the bot's server, and returns its answer.
    json send_offer(
            const smallwebrtc::ConnectionParams& connection,
            const std::string& sdp
    ) {
        APIRequest request;
        request.endpoint = connection.offer_url;
        request.headers = connection.headers;
        request.request_data = {{"sdp", sdp}, {"type", "offer"}};
        if (!connection.request_data.is_null()) {
            request.request_data["requestData"] = connection.request_data;
        }
        request.timeout = OFFER_TIMEOUT;

        HttpResponse response;
        try {
            // Only disconnect() could cancel it, and the client never calls it
            // during connect().
            response = http_post(request, [] { return false; });
        } catch (const std::exception& e) {
            fail(e.what());
        }
        if (response.status < 200 || response.status >= 300) {
            fail("the bot's server answered the offer with HTTP status " +
                 std::to_string(response.status) + ": " + response.body);
        }

        json answer = json::parse(response.body, nullptr, false);
        if (!answer.is_object() || !answer["sdp"].is_string() ||
            !answer["type"].is_string()) {
            fail("the bot's server didn't answer the offer with an SDP answer");
        }
        return answer;
    }

    // Ends a connect() that failed.
    [[noreturn]] void fail(const std::string& error) {
        disconnect();
        throw TransportStartError("Unable to connect to the bot: " + error);
    }

    //
    // libdatachannel callbacks. They run on its threads.
    //

    void handle_open() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state == State::Connecting) {
            _state = State::Open;
            _cv.notify_all();
        }
    }

    // Every connection that ends, or fails, ends up closed.
    void handle_state(rtc::PeerConnection::State state) {
        if (state != rtc::PeerConnection::State::Closed) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(_mutex);
            State previous = _state;
            _state = State::Closed;
            _cv.notify_all();
            // When connecting, connect() reports it. When closing, it was us.
            if (previous != State::Open) {
                return;
            }
        }
        _observer->on_transport_disconnected();
    }

    void handle_message(const std::string& data) {
        json message = json::parse(data, nullptr, false);
        if (!message.is_object()) {
            _observer->on_transport_error(
                    "Invalid message from the bot: not a JSON object", false
            );
            return;
        }
        if (is_type(message, SIGNALLING_TYPE)) {
            handle_signalling(message.value("message", json()));
            return;
        }
        _observer->on_transport_message(message);
    }

    // The bot asks to renegotiate only to fix its video, which this transport
    // doesn't send, so only leaving matters.
    void handle_signalling(const json& message) {
        if (!is_type(message, PEER_LEFT_TYPE)) {
            return;
        }
        Participant bot;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            bot = _bot;
        }
        _observer->on_bot_disconnected(bot);
    }

    TransportObserver* _observer = nullptr;
    // The request that started the bot, if start_bot() did. Only the client's
    // calls, which run one at a time, use it.
    std::optional<APIRequest> _start_request;

    // Guards the connection and its state. _cv wakes up connect() and
    // disconnect() when the state changes, or when ICE gathering ends.
    std::mutex _mutex;
    std::condition_variable _cv;
    std::shared_ptr<rtc::PeerConnection> _pc;
    std::shared_ptr<rtc::Track> _track;
    std::shared_ptr<rtc::DataChannel> _dc;
    State _state = State::Closed;
    // The bot, once connected.
    Participant _bot;
};

SmallWebRTCTransport::SmallWebRTCTransport()
    : _impl(std::make_unique<Impl>()) {}

SmallWebRTCTransport::~SmallWebRTCTransport() = default;

void SmallWebRTCTransport::initialize(TransportObserver* observer) {
    _impl->initialize(observer);
}

void SmallWebRTCTransport::set_start_bot_params(const APIRequest& request) {
    _impl->set_start_bot_params(request);
}

void SmallWebRTCTransport::connect(const json& params) {
    _impl->connect(params);
}

void SmallWebRTCTransport::disconnect() {
    _impl->disconnect();
}

void SmallWebRTCTransport::send_ready_message(const rtvi::Message& message) {
    _impl->send_ready_message(message);
}

void SmallWebRTCTransport::send_message(const rtvi::Message& message) {
    _impl->send_message(message);
}

int32_t SmallWebRTCTransport::send_user_audio(
        const int16_t* /* frames */,
        size_t /* num_frames */
) {
    return 0;
}

int32_t SmallWebRTCTransport::read_bot_audio(
        int16_t* /* frames */,
        size_t /* num_frames */
) {
    return 0;
}

}  // namespace pipecat
