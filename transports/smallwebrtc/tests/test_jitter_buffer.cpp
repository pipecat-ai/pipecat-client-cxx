//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "jitter_buffer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace pipecat::smallwebrtc;
using std::chrono::milliseconds;
using Turns = std::vector<std::string>;

namespace {

const uint32_t CLOCK_RATE = 48000;
// 20 ms packets.
const uint32_t PACKET_DURATION = 960;
const uint32_t FIRST_TIMESTAMP = 1234567;

// When the first packet arrives.
const Clock::time_point START = Clock::time_point() + std::chrono::hours(1);

Clock::time_point at(int64_t ms) {
    return START + milliseconds(ms);
}

// Packet `index`, with its index as payload.
RtpPacket packet(size_t index, uint32_t first_timestamp = FIRST_TIMESTAMP) {
    return {first_timestamp + static_cast<uint32_t>(index) * PACKET_DURATION,
            PACKET_DURATION,
            {static_cast<std::byte>(index)}};
}

// The turn that comes at `ms`: the index of its packet, `lost <duration>`, or
// `-` if none came.
std::string pop(JitterBuffer& buffer, int64_t ms) {
    std::optional<Turn> turn = buffer.pop(at(ms));
    if (!turn) {
        return "-";
    }
    if (turn->packet) {
        return std::to_string(static_cast<int>(turn->packet->payload[0]));
    }
    return "lost " + std::to_string(turn->lost);
}

// The turns that come from `from` to `to` ms, every 20 ms.
Turns pop(JitterBuffer& buffer, int64_t from, int64_t to) {
    Turns turns;
    for (int64_t ms = from; ms <= to; ms += 20) {
        turns.push_back(pop(buffer, ms));
    }
    return turns;
}

}  // namespace

TEST(JitterBuffer, WaitsForTheDelay) {
    JitterBuffer buffer(CLOCK_RATE);
    EXPECT_FALSE(buffer.next_turn());
    buffer.push(packet(0), at(0));
    EXPECT_EQ(buffer.next_turn(), at(PLAYOUT_DELAY.count()));
    EXPECT_EQ(pop(buffer, PLAYOUT_DELAY.count() - 1), "-");
    EXPECT_EQ(pop(buffer, PLAYOUT_DELAY.count()), "0");
}

TEST(JitterBuffer, PlaysAtTheBotsPace) {
    JitterBuffer buffer(CLOCK_RATE);
    // The packets arrive all at once.
    for (size_t i = 0; i < 5; ++i) {
        buffer.push(packet(i), at(0));
    }
    EXPECT_EQ(pop(buffer, 80), "0");
    EXPECT_EQ(pop(buffer, 90), "-");
    EXPECT_EQ(buffer.next_turn(), at(100));
    EXPECT_EQ(pop(buffer, 100, 160), Turns({"1", "2", "3", "4"}));
}

TEST(JitterBuffer, PutsPacketsBackInOrder) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(1), at(0));
    // Before playing starts, so it starts with it, 20 ms earlier.
    buffer.push(packet(0), at(10));
    buffer.push(packet(3), at(20));
    buffer.push(packet(2), at(30));
    EXPECT_EQ(pop(buffer, 80, 140), Turns({"0", "1", "2", "3"}));
}

TEST(JitterBuffer, ConcealsLostPackets) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    buffer.push(packet(2), at(40));
    EXPECT_EQ(pop(buffer, 80, 120), Turns({"0", "lost 960", "2"}));
}

TEST(JitterBuffer, DropsRepeatedPackets) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    buffer.push(packet(1), at(20));
    buffer.push(packet(1), at(21));
    buffer.push(packet(2), at(40));
    EXPECT_EQ(pop(buffer, 80, 120), Turns({"0", "1", "2"}));
}

TEST(JitterBuffer, GrowsTheDelayWhenPacketsArriveLate) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    buffer.push(packet(2), at(40));
    EXPECT_EQ(pop(buffer, 80, 100), Turns({"0", "lost 960"}));

    // Packet 1 arrives 10 ms after its turn: it's dropped, and the next ones
    // play 10 ms later, after 10 ms to conceal.
    buffer.push(packet(1), at(110));
    EXPECT_EQ(pop(buffer, 120), "lost 480");
    EXPECT_EQ(pop(buffer, 125), "-");
    EXPECT_EQ(pop(buffer, 130), "2");
}

TEST(JitterBuffer, CatchesUpAfterTheDelayGrew) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    EXPECT_FALSE(buffer.can_catch_up(at(0)));
    EXPECT_EQ(pop(buffer, 80, 100), Turns({"0", "lost 960"}));
    buffer.push(packet(1), at(110));
    EXPECT_EQ(pop(buffer, 120), "lost 480");

    // Packets have to arrive in time for 2 seconds first.
    EXPECT_FALSE(buffer.can_catch_up(at(1000)));
    EXPECT_TRUE(buffer.can_catch_up(at(2110)));
    EXPECT_EQ(buffer.next_turn(), at(130));
    buffer.catch_up(PACKET_DURATION);
    // By what it grew, not more.
    EXPECT_EQ(buffer.next_turn(), at(120));
    EXPECT_FALSE(buffer.can_catch_up(at(2110)));
}

TEST(JitterBuffer, StopsWhenTheBotStopsSending) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    // Up to 100 ms are concealed.
    EXPECT_EQ(
            pop(buffer, 80, 200),
            Turns({"0",
                   "lost 960",
                   "lost 960",
                   "lost 960",
                   "lost 960",
                   "lost 960",
                   "-"})
    );
    EXPECT_FALSE(buffer.next_turn());

    // When the bot sends again, it plays after the delay.
    buffer.push(packet(1), at(1000));
    EXPECT_EQ(buffer.next_turn(), at(1080));
    EXPECT_EQ(pop(buffer, 1080), "1");
}

TEST(JitterBuffer, JumpsAheadWithoutConcealing) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    // 400 ms later, which is too long for lost packets.
    buffer.push(packet(20), at(20));
    EXPECT_EQ(pop(buffer, 80, 100), Turns({"0", "20"}));
}

TEST(JitterBuffer, StartsOverWhenTheBotDoes) {
    JitterBuffer buffer(CLOCK_RATE);
    buffer.push(packet(0), at(0));
    buffer.push(packet(1), at(20));
    EXPECT_EQ(pop(buffer, 80, 100), Turns({"0", "1"}));

    // Ten seconds earlier: it plays after the delay.
    buffer.push(packet(5, FIRST_TIMESTAMP - 10 * CLOCK_RATE), at(110));
    EXPECT_EQ(buffer.next_turn(), at(190));
    EXPECT_EQ(pop(buffer, 190), "5");
}

TEST(JitterBuffer, HoldsUpToASecond) {
    JitterBuffer buffer(CLOCK_RATE);
    // Two seconds of audio that nobody plays.
    for (size_t i = 0; i < 100; ++i) {
        buffer.push(packet(i), at(static_cast<int64_t>(i) * 20));
    }
    // The first second was dropped.
    EXPECT_EQ(pop(buffer, 2000), "50");
}

TEST(JitterBuffer, TimestampsWrapAround) {
    JitterBuffer buffer(CLOCK_RATE);
    // They wrap around after packet 0.
    uint32_t first = UINT32_MAX - PACKET_DURATION + 1;
    buffer.push(packet(0, first), at(0));
    buffer.push(packet(2, first), at(10));
    buffer.push(packet(1, first), at(20));
    EXPECT_EQ(pop(buffer, 80, 120), Turns({"0", "1", "2"}));
}
