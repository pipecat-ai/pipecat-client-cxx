//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_SMALLWEBRTC_JITTER_BUFFER_H
#define PIPECAT_SMALLWEBRTC_JITTER_BUFFER_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace pipecat::smallwebrtc {

using Clock = std::chrono::steady_clock;

// How long after the bot's first packet arrives its audio starts playing, so
// the packets that arrive late, or out of order, still make it in their turn.
const std::chrono::milliseconds PLAYOUT_DELAY {80};

// An RTP packet's payload, its timestamp, and how long it plays, in
// timestamp units.
struct RtpPacket {
    uint32_t timestamp;
    uint32_t duration;
    std::vector<std::byte> payload;
};

// What plays next: a packet, or, when packets were lost, how long to conceal,
// in timestamp units.
struct Turn {
    std::optional<RtpPacket> packet;
    uint32_t lost = 0;
};

// Plays the bot's packets in order, at the pace the bot sent them, a delay
// after they arrive, so the network's ups and downs don't reach the app:
//
// - The delay starts at PLAYOUT_DELAY. A packet that arrives after its turn is
//   dropped, and the delay grows by how late it was, up to 300 ms, so the
//   next ones make it. The extra time is concealed. catch_up() shrinks it
//   back.
// - A packet that hasn't arrived by its turn was lost: its time is left to
//   conceal, up to 100 ms. After that, the bot stopped sending, and playing
//   starts over when it sends again.
// - It holds up to a second of audio, for apps that don't play it.
//
// Not thread-safe.
class JitterBuffer {
   public:
    // `clock_rate` is how many timestamp units make a second.
    explicit JitterBuffer(uint32_t clock_rate);

    // Adds a packet that arrived at `now`.
    void push(RtpPacket packet, Clock::time_point now);

    // The next turn, if it came by `now`.
    std::optional<Turn> pop(Clock::time_point now);

    // When the next turn comes. Nothing if it waits for a packet.
    std::optional<Clock::time_point> next_turn() const;

    // Whether the delay grew, and packets have arrived in time for a while,
    // so it can shrink back.
    bool can_catch_up(Clock::time_point now) const;

    // Shrinks the delay by up to `duration`, in timestamp units, after
    // skipping that much audio, e.g. silence.
    void catch_up(uint32_t duration);

   private:
    Clock::duration time_of(int64_t duration) const;
    int64_t duration_of(Clock::duration time) const;

    const uint32_t _clock_rate;
    // Packets waiting for their turn, oldest first.
    std::vector<RtpPacket> _held;
    // The timestamp of the next turn, and when it plays. Nothing until the
    // bot sends.
    std::optional<uint32_t> _next_timestamp;
    Clock::time_point _next_time;
    // Audio to conceal before the next turn, after the delay grew.
    uint32_t _stretch = 0;
    Clock::duration _delay = PLAYOUT_DELAY;
    // When a packet last arrived late.
    Clock::time_point _late_time;
    // How long the last packet played, and how much was concealed since.
    uint32_t _last_duration;
    uint32_t _concealed = 0;
};

}  // namespace pipecat::smallwebrtc

#endif
