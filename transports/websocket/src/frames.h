//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_WEBSOCKET_FRAMES_H
#define PIPECAT_WEBSOCKET_FRAMES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// The frames of Pipecat's frames.proto that the transport sends and receives,
// encoded and decoded in the protobuf wire format.
namespace pipecat::websocket {

// An RTVI message, as JSON.
struct MessageFrame {
    std::string data;
};

// Interleaved 16-bit PCM audio.
struct AudioFrame {
    std::vector<int16_t> samples;
    uint32_t sample_rate = 0;
    uint32_t num_channels = 0;
};

// The bot was interrupted, so the audio it already sent shouldn't play.
struct InterruptionFrame {};

// A frame the transport doesn't use, e.g. text or a transcription.
struct OtherFrame {};

using Frame =
        std::variant<MessageFrame, AudioFrame, InterruptionFrame, OtherFrame>;

// Encodes an RTVI message.
std::string encode_message(const std::string& json);

// Encodes `num_frames` frames of interleaved audio.
std::string encode_audio(
        const int16_t* frames,
        size_t num_frames,
        uint32_t sample_rate,
        uint32_t num_channels
);

// Decodes a frame. Throws std::runtime_error if `data` isn't a valid frame.
Frame decode(const void* data, size_t size);

}  // namespace pipecat::websocket

#endif
