//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// A transport that connects to Pipecat bots over Daily.

#ifndef PIPECAT_DAILY_TRANSPORT_H
#define PIPECAT_DAILY_TRANSPORT_H

#include <pipecat/transport.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pipecat {

/// Options to create a DailyTransport.
struct DailyTransportOptions {
    /// Sample rate of the user audio you send, in Hz.
    uint32_t user_audio_sample_rate = 16000;
    /// Number of channels of the user audio you send.
    uint8_t user_audio_channels = 1;
    /// Sample rate of the bot audio you read, in Hz.
    uint32_t bot_audio_sample_rate = 16000;
    /// Number of channels of the bot audio you read.
    uint8_t bot_audio_channels = 1;
};

/// Connects to Pipecat bots over [Daily](https://www.daily.co) (WebRTC).
///
/// Give it to PipecatClientOptions::transport. To connect, it needs the
/// URL of the bot's Daily room, and a token if the room needs one. Start
/// endpoints like Pipecat Cloud return both, as `dailyRoom` and `dailyToken`.
///
/// You can use several at the same time, e.g. one per client to talk to
/// several bots.
class DailyTransport : public Transport {
   public:
    /// Creates a transport.
    explicit DailyTransport(DailyTransportOptions options = {});

    /// Disconnects if needed.
    ~DailyTransport() override;

    /// Transports can't be copied.
    DailyTransport(const DailyTransport&) = delete;

    /// Transports can't be copied.
    DailyTransport& operator=(const DailyTransport&) = delete;

    /// Prepares Daily.
    void initialize(TransportObserver* observer) override;

    /// Joins the bot's Daily room.
    ///
    /// `params` has the room URL in `url` (or `dailyRoom` or `room_url`), and
    /// the token, if the room needs one, in `token` (or `dailyToken`). Throws
    /// InvalidTransportParamsError if there's no URL, and TransportStartError
    /// if joining fails.
    void connect(const nlohmann::json& params) override;

    /// Leaves the room.
    void disconnect() override;

    /// Sends the `client-ready` message once the bot's audio is playing.
    void send_ready_message(const rtvi::Message& message) override;

    /// Sends a message to the bot.
    void send_message(const rtvi::Message& message) override;

    /// Sends `num_frames` frames of 16-bit PCM user audio, in the format of
    /// the options. Returns the number of frames sent.
    ///
    /// Send the audio continuously, as it's captured, and send silence while
    /// the user is muted: the bot needs it to tell when the user stops
    /// speaking. The audio is sent as is, without echo cancellation.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames) override;

    /// Reads up to `num_frames` frames of 16-bit PCM bot audio, in the format
    /// of the options. Waits until there's audio to read, or until the
    /// transport disconnects. Returns the number of frames read, or 0 if it
    /// disconnected.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames) override;

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
