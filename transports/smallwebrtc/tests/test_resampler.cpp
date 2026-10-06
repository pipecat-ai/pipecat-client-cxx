//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "resampler.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace pipecat::smallwebrtc;
using Samples = std::vector<int16_t>;

namespace {

// M_PI isn't standard.
const double PI = 3.14159265358979323846;

// A 440 Hz tone, `channels` times per frame.
Samples tone(size_t num_frames, uint32_t sample_rate, uint32_t channels = 1) {
    Samples samples;
    for (size_t i = 0; i < num_frames; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        auto sample = static_cast<int16_t>(10000 * std::sin(2 * PI * 440 * t));
        samples.insert(samples.end(), channels, sample);
    }
    return samples;
}

// Converts a second of audio, 20 ms at a time.
Samples convert_a_second(
        Resampler& resampler,
        const Samples& samples,
        uint32_t channels,
        uint32_t sample_rate
) {
    size_t chunk = sample_rate / 50;
    Samples out;
    for (size_t frame = 0; frame < sample_rate; frame += chunk) {
        Samples converted = resampler.process(
                samples.data() + frame * channels, chunk, channels, sample_rate
        );
        out.insert(out.end(), converted.begin(), converted.end());
    }
    return out;
}

// Counts the zero crossings of 16 kHz audio, after its first 20 ms: the tone
// starts abruptly, so the resampler rings there.
size_t zero_crossings(const Samples& samples) {
    size_t crossings = 0;
    for (size_t i = 321; i < samples.size(); ++i) {
        crossings += (samples[i - 1] < 0) != (samples[i] < 0);
    }
    return crossings;
}

}  // namespace

TEST(Resampler, KeepsAudioAtTheSameSampleRate) {
    Resampler resampler(16000);
    Samples samples {1, 2, 3, 4};

    EXPECT_EQ(resampler.process(samples.data(), 2, 2, 16000), samples);
}

TEST(Resampler, ConvertsTheSampleRate) {
    Resampler resampler(16000);

    Samples out = convert_a_second(resampler, tone(24000, 24000), 1, 24000);

    EXPECT_EQ(out.size(), 16000u);
    // Still 440 Hz: 880 crossings a second.
    EXPECT_NEAR(zero_crossings(out), 862, 2);
}

TEST(Resampler, KeepsChannelsInterleaved) {
    Resampler resampler(16000);

    Samples out = convert_a_second(resampler, tone(48000, 48000, 2), 2, 48000);

    ASSERT_EQ(out.size(), 32000u);
    Samples left, right;
    for (size_t i = 0; i < out.size(); i += 2) {
        left.push_back(out[i]);
        right.push_back(out[i + 1]);
    }
    EXPECT_EQ(left, right);
    EXPECT_NEAR(zero_crossings(left), 862, 2);
}

TEST(Resampler, ResetForgetsTheAudio) {
    Resampler resampler(16000);
    Samples loud = tone(480, 24000);
    Samples silence(480);

    resampler.process(loud.data(), loud.size(), 1, 24000);
    resampler.reset();

    for (int16_t sample: resampler.process(silence.data(), 480, 1, 24000)) {
        EXPECT_EQ(sample, 0);
    }
}
