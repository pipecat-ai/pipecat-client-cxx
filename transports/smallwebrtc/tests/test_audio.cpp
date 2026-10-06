//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "audio.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace pipecat::smallwebrtc;
using Samples = std::vector<int16_t>;
using Packets = std::vector<std::vector<std::byte>>;

namespace {

// M_PI isn't standard.
const double PI = 3.14159265358979323846;

// A second of a 440 Hz tone, `channels` times per frame.
Samples tone(uint32_t sample_rate, uint32_t channels = 1) {
    Samples samples;
    for (size_t i = 0; i < sample_rate; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        auto sample = static_cast<int16_t>(10000 * std::sin(2 * PI * 440 * t));
        samples.insert(samples.end(), channels, sample);
    }
    return samples;
}

// Encodes `samples`, `chunk` frames at a time, like an app would send them.
Packets encode(
        AudioEncoder& encoder,
        const Samples& samples,
        uint32_t channels,
        size_t chunk
) {
    Packets packets;
    size_t num_frames = samples.size() / channels;
    for (size_t frame = 0; frame < num_frames; frame += chunk) {
        size_t count = std::min(chunk, num_frames - frame);
        Packets encoded =
                encoder.encode(samples.data() + frame * channels, count);
        packets.insert(packets.end(), encoded.begin(), encoded.end());
    }
    return packets;
}

Samples decode(AudioDecoder& decoder, const Packets& packets) {
    Samples samples;
    for (const auto& packet: packets) {
        Samples decoded = decoder.decode(packet.data(), packet.size());
        samples.insert(samples.end(), decoded.begin(), decoded.end());
    }
    return samples;
}

// Checks that mono audio is still the 440 Hz tone, which crosses zero 880
// times a second. Its first 100 ms are skipped: the codec and the resampler
// take a while to settle.
void expect_tone(const Samples& samples, uint32_t sample_rate) {
    size_t start = sample_rate / 10;
    ASSERT_GT(samples.size(), start);
    size_t crossings = 0;
    for (size_t i = start + 1; i < samples.size(); ++i) {
        crossings += (samples[i - 1] < 0) != (samples[i] < 0);
    }
    double seconds = static_cast<double>(samples.size() - start) / sample_rate;
    EXPECT_NEAR(static_cast<double>(crossings), 880 * seconds, 10);
}

}  // namespace

TEST(AudioEncoder, MakesPacketsOf20Milliseconds) {
    AudioEncoder encoder(16000, 1);
    // Chunks that don't match packets: what's left waits for the next one.
    Packets packets = encode(encoder, tone(16000), 1, 333);
    EXPECT_EQ(packets.size(), 50u);
    for (const auto& packet: packets) {
        EXPECT_GT(packet.size(), 0u);
    }
    // A packet's worth more completes one more packet.
    Samples more(320);
    EXPECT_EQ(encoder.encode(more.data(), more.size()).size(), 1u);
}

TEST(AudioEncoder, RoundTrip) {
    AudioEncoder encoder(16000, 1);
    AudioDecoder decoder(16000);
    Samples decoded = decode(decoder, encode(encoder, tone(16000), 1, 160));

    EXPECT_EQ(decoded.size(), 16000u);
    expect_tone(decoded, 16000);
}

TEST(AudioEncoder, RoundTripAtOtherSampleRates) {
    // 44.1 kHz isn't an Opus sample rate, so the audio is converted, and
    // stereo is mixed down to mono.
    AudioEncoder encoder(44100, 2);
    AudioDecoder decoder(44100);
    Samples decoded = decode(decoder, encode(encoder, tone(44100, 2), 2, 441));

    // Up to a packet waits for more audio.
    EXPECT_LE(decoded.size(), 44100u);
    EXPECT_GE(decoded.size(), 44100u - 2 * 882);
    expect_tone(decoded, 44100);
}

TEST(AudioDecoder, IgnoresInvalidPackets) {
    AudioDecoder decoder(16000);
    std::vector<std::byte> packet {std::byte {0xff}};
    EXPECT_TRUE(decoder.decode(packet.data(), packet.size()).empty());
}
