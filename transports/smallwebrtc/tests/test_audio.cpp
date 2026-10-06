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

// RTP timestamps of 20 ms packets.
const uint32_t FIRST_TIMESTAMP = 1234567;
const uint32_t PACKET_TICKS = 960;

// Packets 0 to `count` - 1, in order.
std::vector<size_t> in_order(size_t count) {
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; ++i) {
        order[i] = i;
    }
    return order;
}

// Decodes `packets` as they'd arrive: `order` has their indexes, and each has
// the timestamp of its index.
Samples decode(
        AudioDecoder& decoder,
        const Packets& packets,
        const std::vector<size_t>& order,
        uint32_t first_timestamp = FIRST_TIMESTAMP
) {
    Samples samples;
    for (size_t i: order) {
        auto timestamp =
                first_timestamp + static_cast<uint32_t>(i) * PACKET_TICKS;
        Samples decoded =
                decoder.decode(packets[i].data(), packets[i].size(), timestamp);
        samples.insert(samples.end(), decoded.begin(), decoded.end());
    }
    return samples;
}

Samples decode(AudioDecoder& decoder, const Packets& packets) {
    return decode(decoder, packets, in_order(packets.size()));
}

// Removes packets from `order`.
std::vector<size_t>
without(std::vector<size_t> order, const std::vector<size_t>& lost) {
    for (size_t packet: lost) {
        order.erase(std::find(order.begin(), order.end(), packet));
    }
    return order;
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

TEST(AudioDecoder, ConcealsLostPackets) {
    AudioEncoder encoder(16000, 1);
    Packets packets = encode(encoder, tone(16000), 1, 320);
    AudioDecoder decoder(16000);
    Samples decoded =
            decode(decoder, packets, without(in_order(50), {10, 20, 21}));

    // The lost 60 ms are still there, so the audio plays in time.
    EXPECT_EQ(decoded.size(), 16000u);
    expect_tone(decoded, 16000);
}

TEST(AudioDecoder, DropsLateAndRepeatedPackets) {
    AudioEncoder encoder(16000, 1);
    Packets packets = encode(encoder, tone(16000), 1, 320);
    AudioDecoder decoder(16000);
    Samples expected = decode(decoder, packets);

    std::vector<size_t> order = in_order(50);
    // Packet 5 again, after 12, and 30 twice.
    order.insert(order.begin() + 13, 5);
    order.insert(order.begin() + 32, 30);
    AudioDecoder other(16000);
    EXPECT_EQ(decode(other, packets, order), expected);
}

TEST(AudioDecoder, TimestampsWrapAround) {
    AudioEncoder encoder(16000, 1);
    Packets packets = encode(encoder, tone(16000), 1, 320);
    AudioDecoder decoder(16000);
    // The timestamps wrap around after packet 10, and packet 12 is lost.
    uint32_t first = UINT32_MAX - 10 * PACKET_TICKS + 1;
    Samples decoded =
            decode(decoder, packets, without(in_order(50), {12}), first);
    EXPECT_EQ(decoded.size(), 16000u);
}

TEST(AudioDecoder, DoesNotConcealJumps) {
    AudioEncoder encoder(16000, 1);
    Packets packets = encode(encoder, tone(16000), 1, 320);
    AudioDecoder decoder(16000);
    Samples decoded;
    for (size_t i = 0; i < packets.size(); ++i) {
        // A second later after packet 25, which isn't lost audio.
        uint32_t timestamp = FIRST_TIMESTAMP +
                             static_cast<uint32_t>(i) * PACKET_TICKS +
                             (i >= 25 ? 48000 : 0);
        Samples samples =
                decoder.decode(packets[i].data(), packets[i].size(), timestamp);
        decoded.insert(decoded.end(), samples.begin(), samples.end());
    }
    EXPECT_EQ(decoded.size(), 16000u);
}

TEST(AudioDecoder, IgnoresInvalidPackets) {
    AudioDecoder decoder(16000);
    std::vector<std::byte> packet {std::byte {0xff}};
    EXPECT_TRUE(decoder.decode(packet.data(), packet.size(), FIRST_TIMESTAMP)
                        .empty());
}
