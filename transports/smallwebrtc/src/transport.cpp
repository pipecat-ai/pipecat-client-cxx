//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/smallwebrtc/transport.h"

#include "audio.h"
#include "http.h"
#include "jitter_buffer.h"
#include "params.h"

#include <pipecat/client.h>
#include <pipecat/errors.h>

#include <rtc/rtc.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

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
const char* AUDIO_CNAME = "pipecat";
// The bot finds the user's audio on the first transceiver.
const int AUDIO_TRANSCEIVER_INDEX = 0;

// Samples quieter than this are silence.
const int16_t SILENCE = 32;

// Messages about the connection itself, with the bot.
const char* SIGNALLING_TYPE = "signalling";
const char* PEER_LEFT_TYPE = "peerLeft";
const char* TRACK_STATUS_TYPE = "trackStatus";

// Whether `message` is a JSON object of type `type`.
bool is_type(const json& message, const char* type) {
    if (!message.is_object()) {
        return false;
    }
    auto it = message.find("type");
    return it != message.end() && *it == type;
}

// Tells the bot whether the user's audio is on, like client-js does with the
// microphone.
void send_audio_status(rtc::DataChannel& dc, bool enabled) {
    json status = {
            {"type", TRACK_STATUS_TYPE},
            {"receiver_index", AUDIO_TRANSCEIVER_INDEX},
            {"enabled", enabled},
    };
    try {
        dc.send(json({{"type", SIGNALLING_TYPE}, {"message", status}}).dump());
    } catch (const std::exception&) {
        // It's closing, which is reported on its own.
    }
}

}  // namespace

class SmallWebRTCTransport::Impl {
   public:
    //
    // Connection
    //

    explicit Impl(SmallWebRTCTransportOptions options) : _options(options) {}

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

        uint32_t ssrc = std::random_device {}();
        rtc::Description::Audio audio(
                "0", rtc::Description::Direction::SendRecv
        );
        audio.addOpusCodec(OPUS_PAYLOAD_TYPE);
        audio.addSSRC(ssrc, AUDIO_CNAME);
        auto track = pc->addTrack(audio);

        // Packetizes the user's audio, and depacketizes the bot's.
        auto rtp_config = std::make_shared<rtc::RtpPacketizationConfig>(
                ssrc,
                AUDIO_CNAME,
                OPUS_PAYLOAD_TYPE,
                rtc::OpusRtpPacketizer::DefaultClockRate
        );
        auto rtp = std::make_shared<rtc::OpusRtpPacketizer>(rtp_config);
        rtp->addToChain(std::make_shared<rtc::OpusRtpDepacketizer>());
        track->setMediaHandler(rtp);

        track->onFrame([this](rtc::binary payload, rtc::FrameInfo info) {
            receive_bot_audio(std::move(payload), info.timestamp);
        });

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
        _encoder = std::make_unique<smallwebrtc::AudioEncoder>(
                _options.user_audio_sample_rate, _options.user_audio_channels
        );
        _sent_packets = 0;
        lock.unlock();

        // The bot can speak as soon as it's connected.
        start_bot_audio();

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
        stop_bot_audio();
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

    //
    // Audio
    //

    int32_t send_user_audio(const int16_t* frames, size_t num_frames) {
        std::shared_ptr<rtc::Track> track;
        std::shared_ptr<rtc::DataChannel> dc;
        std::vector<std::vector<std::byte>> packets;
        int64_t first_packet = 0;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state != State::Open) {
                return 0;
            }
            packets = _encoder->encode(frames, num_frames);
            first_packet = _sent_packets;
            _sent_packets += static_cast<int64_t>(packets.size());
            track = _track;
            dc = _dc;
        }

        if (first_packet == 0 && !packets.empty()) {
            send_audio_status(*dc, true);
        }
        for (size_t i = 0; i < packets.size(); ++i) {
            std::chrono::duration<double> timestamp =
                    smallwebrtc::PACKET_DURATION *
                    (first_packet + static_cast<int64_t>(i));
            try {
                track->sendFrame(std::move(packets[i]), timestamp);
            } catch (const std::exception&) {
                // It's closing, which is reported on its own.
            }
        }
        return static_cast<int32_t>(num_frames);
    }

    int32_t read_bot_audio(int16_t* frames, size_t num_frames) {
        size_t channels = _options.bot_audio_channels;
        std::unique_lock<std::mutex> lock(_audio_mutex);
        while (_bot_audio.size() < num_frames * channels) {
            if (!_receiving) {
                return 0;
            }
            std::optional<smallwebrtc::Turn> turn =
                    _jitter_buffer->pop(smallwebrtc::Clock::now());
            if (!turn) {
                // Wait for the next turn, or for a packet if there's none.
                if (auto next = _jitter_buffer->next_turn()) {
                    _audio_cv.wait_until(lock, *next);
                } else {
                    _audio_cv.wait(lock);
                }
                continue;
            }

            std::vector<int16_t> samples;
            if (turn->packet) {
                const auto& payload = turn->packet->payload;
                samples = _decoder->decode(payload.data(), payload.size());
                // Skip silence to shrink the delay back after it grew.
                bool silent = std::all_of(
                        samples.begin(), samples.end(), [](int16_t sample) {
                            return sample > -SILENCE && sample < SILENCE;
                        }
                );
                auto now = smallwebrtc::Clock::now();
                if (silent && _jitter_buffer->can_catch_up(now)) {
                    _jitter_buffer->catch_up(turn->packet->duration);
                    continue;
                }
            } else {
                samples = _decoder->conceal(turn->lost);
            }
            for (int16_t sample: samples) {
                _bot_audio.insert(_bot_audio.end(), channels, sample);
            }
        }

        auto end = _bot_audio.begin() +
                   static_cast<std::ptrdiff_t>(num_frames * channels);
        std::copy(_bot_audio.begin(), end, frames);
        _bot_audio.erase(_bot_audio.begin(), end);
        return static_cast<int32_t>(num_frames);
    }

   private:
    //
    // The bot's audio
    //

    void start_bot_audio() {
        std::lock_guard<std::mutex> lock(_audio_mutex);
        _jitter_buffer.emplace(smallwebrtc::RTP_CLOCK_RATE);
        _decoder = std::make_unique<smallwebrtc::AudioDecoder>(
                _options.bot_audio_sample_rate
        );
        _bot_audio.clear();
        _receiving = true;
    }

    void stop_bot_audio() {
        std::lock_guard<std::mutex> lock(_audio_mutex);
        _receiving = false;
        _audio_cv.notify_all();
    }

    // Runs on libdatachannel's thread, for every packet of the bot's audio.
    void receive_bot_audio(rtc::binary payload, uint32_t timestamp) {
        uint32_t duration = smallwebrtc::AudioDecoder::duration(
                payload.data(), payload.size()
        );
        if (duration == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(_audio_mutex);
        if (_receiving) {
            _jitter_buffer->push(
                    {timestamp, duration, std::move(payload)},
                    smallwebrtc::Clock::now()
            );
            _audio_cv.notify_all();
        }
    }

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
        std::unique_lock<std::mutex> lock(_mutex);
        if (_state != State::Connecting) {
            return;
        }
        std::shared_ptr<rtc::DataChannel> dc = _dc;
        lock.unlock();

        // The user's audio is off until the app sends some. Sent before the
        // connection opens, so it can't arrive after the audio turns it on.
        send_audio_status(*dc, false);

        lock.lock();
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
        stop_bot_audio();
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

    SmallWebRTCTransportOptions _options;
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
    // Encodes the user's audio, and counts the packets sent, for their
    // timestamps.
    std::unique_ptr<smallwebrtc::AudioEncoder> _encoder;
    int64_t _sent_packets = 0;

    // The bot's audio. Guarded by _audio_mutex. _audio_cv wakes up
    // read_bot_audio() when a packet arrives, or when the transport
    // disconnects.
    std::mutex _audio_mutex;
    std::condition_variable _audio_cv;
    // Whether the bot's audio arrives, with the jitter buffer and decoder of
    // the connection.
    bool _receiving = false;
    std::optional<smallwebrtc::JitterBuffer> _jitter_buffer;
    std::unique_ptr<smallwebrtc::AudioDecoder> _decoder;
    // Audio played, in the options' format, the app hasn't read yet.
    std::deque<int16_t> _bot_audio;
};

SmallWebRTCTransport::SmallWebRTCTransport(SmallWebRTCTransportOptions options)
    : _impl(std::make_unique<Impl>(options)) {}

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
        const int16_t* frames,
        size_t num_frames
) {
    return _impl->send_user_audio(frames, num_frames);
}

int32_t
SmallWebRTCTransport::read_bot_audio(int16_t* frames, size_t num_frames) {
    return _impl->read_bot_audio(frames, num_frames);
}

}  // namespace pipecat
