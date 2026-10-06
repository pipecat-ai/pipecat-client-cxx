//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_SMALLWEBRTC_RESAMPLER_H
#define PIPECAT_SMALLWEBRTC_RESAMPLER_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct SpeexResamplerState_;

namespace pipecat::smallwebrtc {

// Converts 16-bit PCM audio to one sample rate, with speexdsp. It keeps the
// end of each audio it converts, to convert the next one smoothly. Not
// thread-safe.
class Resampler {
   public:
    // Converts audio to `sample_rate`.
    explicit Resampler(uint32_t sample_rate);
    ~Resampler();

    Resampler(const Resampler&) = delete;
    Resampler& operator=(const Resampler&) = delete;

    // Converts `num_frames` frames of `channels` interleaved channels at
    // `sample_rate`, and returns them, interleaved too.
    std::vector<int16_t> process(
            const int16_t* samples,
            size_t num_frames,
            uint32_t channels,
            uint32_t sample_rate
    );

    // Forgets the audio it keeps, e.g. when the bot is interrupted.
    void reset();

   private:
    const uint32_t _sample_rate;

    // speexdsp's resampler, for the channels and sample rate of the last
    // audio that needed one.
    SpeexResamplerState_* _state = nullptr;
    uint32_t _channels = 0;
    uint32_t _input_sample_rate = 0;
};

}  // namespace pipecat::smallwebrtc

#endif
