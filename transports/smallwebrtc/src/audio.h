//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_SMALLWEBRTC_AUDIO_H
#define PIPECAT_SMALLWEBRTC_AUDIO_H

#include "resampler.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

struct OpusEncoder;
struct OpusDecoder;

namespace pipecat::smallwebrtc {

// How much audio each packet AudioEncoder makes has.
const std::chrono::milliseconds PACKET_DURATION {20};

// Encodes the user's 16-bit PCM audio into Opus packets of PACKET_DURATION,
// in mono, which is what the bot listens to. Not thread-safe.
class AudioEncoder {
   public:
    // Encodes audio at `sample_rate`, with `channels` interleaved channels.
    // Throws std::runtime_error if Opus can't be set up.
    AudioEncoder(uint32_t sample_rate, uint32_t channels);
    ~AudioEncoder();

    AudioEncoder(const AudioEncoder&) = delete;
    AudioEncoder& operator=(const AudioEncoder&) = delete;

    // Adds `num_frames` frames, and returns the packets they complete. What's
    // left waits for the next frames.
    std::vector<std::vector<std::byte>>
    encode(const int16_t* samples, size_t num_frames);

   private:
    const uint32_t _sample_rate;
    const uint32_t _channels;
    // The audio converted to a sample rate Opus supports.
    const uint32_t _opus_sample_rate;
    Resampler _resampler;
    OpusEncoder* _encoder = nullptr;
    // Mono audio, at _opus_sample_rate, that doesn't fill a packet yet.
    std::vector<int16_t> _pending;
};

// Opus' RTP timestamps count at 48 kHz, whatever the audio's sample rate.
const uint32_t RTP_CLOCK_RATE = 48000;

// Decodes the bot's Opus packets into mono 16-bit PCM, and conceals the ones
// that were lost. Not thread-safe.
class AudioDecoder {
   public:
    // Decodes audio to `sample_rate`. Throws std::runtime_error if Opus can't
    // be set up.
    explicit AudioDecoder(uint32_t sample_rate);
    ~AudioDecoder();

    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;

    // How long a packet plays, in RTP timestamp units, or 0 if it's invalid.
    static uint32_t duration(const std::byte* data, size_t size);

    // Decodes a packet. Returns nothing if it's invalid.
    std::vector<int16_t> decode(const std::byte* data, size_t size);

    // Makes up `duration` of audio, in RTP timestamp units, from the audio
    // before it, where packets were lost. Opus does it in steps of 2.5 ms, so
    // the rest is left out.
    std::vector<int16_t> conceal(uint32_t duration);

   private:
    // Decodes `size` bytes of `data`, or conceals if there's no data, into
    // `frames` frames, and converts them.
    std::vector<int16_t>
    decode(const std::byte* data, size_t size, size_t frames);

    // The audio decoded at a sample rate Opus supports, then converted.
    const uint32_t _opus_sample_rate;
    Resampler _resampler;
    OpusDecoder* _decoder = nullptr;
};

}  // namespace pipecat::smallwebrtc

#endif
