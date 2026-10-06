//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "jitter_buffer.h"

#include <algorithm>

namespace pipecat::smallwebrtc {

namespace {

// The longest the delay grows.
const std::chrono::milliseconds MAX_DELAY {300};
// How long packets have to arrive in time for the delay to shrink back.
const std::chrono::seconds CATCH_UP_AFTER {2};
// The most audio concealed in a row. After that, the bot stopped sending.
const std::chrono::milliseconds MAX_CONCEALED {100};
// A packet this late means the bot started its audio over, like aiortc's
// MAX_MISORDER.
const std::chrono::seconds MAX_LATE {2};
// The most audio held, for apps that don't play it.
const std::chrono::seconds MAX_HELD {1};

// How much `timestamp` comes after `other`, which is negative if it comes
// before. Timestamps wrap around, so it's their difference as signed.
int64_t after(uint32_t timestamp, uint32_t other) {
    return static_cast<int32_t>(timestamp - other);
}

}  // namespace

JitterBuffer::JitterBuffer(uint32_t clock_rate)
    : _clock_rate(clock_rate),
      // Packets usually play for 20 ms.
      _last_duration(clock_rate / 50) {}

void JitterBuffer::push(RtpPacket packet, Clock::time_point now) {
    int64_t early =
            _next_timestamp ? after(packet.timestamp, *_next_timestamp) : 0;
    if (early < 0 && -early >= duration_of(MAX_LATE)) {
        // The bot started its audio over.
        _held.clear();
        _next_timestamp.reset();
        _stretch = 0;
    } else if (early < 0) {
        Clock::time_point turn = _next_time - time_of(-early);
        if (turn > now) {
            // It came out of order before playing started: start with it.
            _next_timestamp = packet.timestamp;
            _next_time = turn;
        } else {
            // Its turn passed: give the next packets more time. The extra
            // time is concealed, in Opus' steps of 2.5 ms.
            auto grow =
                    std::min<Clock::duration>(now - turn, MAX_DELAY - _delay);
            int64_t step = _clock_rate / 400;
            int64_t stretch = (duration_of(grow) + step - 1) / step * step;
            if (stretch > 0) {
                _stretch += static_cast<uint32_t>(stretch);
                _delay += time_of(stretch);
            }
            _late_time = now;
            return;
        }
    }

    auto turn = std::find_if(_held.begin(), _held.end(), [&](const auto& held) {
        return after(held.timestamp, packet.timestamp) >= 0;
    });
    if (turn != _held.end() && turn->timestamp == packet.timestamp) {
        return;
    }
    _held.insert(turn, std::move(packet));

    if (!_next_timestamp) {
        _next_timestamp = _held.front().timestamp;
        _next_time = now + _delay;
        _concealed = 0;
    }

    // Skip ahead if the app doesn't play the audio.
    const RtpPacket* last = &_held.back();
    while (after(last->timestamp + last->duration, *_next_timestamp) >
           duration_of(MAX_HELD)) {
        _held.erase(_held.begin());
        _next_timestamp = _held.front().timestamp;
        last = &_held.back();
    }
}

std::optional<Turn> JitterBuffer::pop(Clock::time_point now) {
    if (!_next_timestamp || now < _next_time) {
        return std::nullopt;
    }

    if (_stretch > 0) {
        Turn turn {std::nullopt, _stretch};
        _next_time += time_of(_stretch);
        _stretch = 0;
        return turn;
    }

    int64_t gap = 0;
    if (!_held.empty()) {
        gap = after(_held.front().timestamp, *_next_timestamp);
        // A jump ahead, not lost packets.
        if (gap > duration_of(MAX_CONCEALED)) {
            _next_timestamp = _held.front().timestamp;
            gap = 0;
        }
        if (gap == 0) {
            Turn turn {std::move(_held.front())};
            _held.erase(_held.begin());
            _last_duration = turn.packet->duration;
            *_next_timestamp += _last_duration;
            _next_time += time_of(_last_duration);
            _concealed = 0;
            return turn;
        }
    }

    // Its packet didn't arrive in time.
    if (_held.empty() && _concealed >= duration_of(MAX_CONCEALED)) {
        // The bot stopped sending, so play again when it sends again.
        _next_timestamp.reset();
        return std::nullopt;
    }
    uint32_t lost = _last_duration;
    if (!_held.empty()) {
        lost = std::min(lost, static_cast<uint32_t>(gap));
    }
    *_next_timestamp += lost;
    _next_time += time_of(lost);
    _concealed += lost;
    return Turn {std::nullopt, lost};
}

std::optional<Clock::time_point> JitterBuffer::next_turn() const {
    if (!_next_timestamp) {
        return std::nullopt;
    }
    return _next_time;
}

bool JitterBuffer::can_catch_up(Clock::time_point now) const {
    return _delay > PLAYOUT_DELAY && now - _late_time >= CATCH_UP_AFTER;
}

void JitterBuffer::catch_up(uint32_t duration) {
    auto shrink = std::min<Clock::duration>(
            time_of(duration), _delay - PLAYOUT_DELAY
    );
    _delay -= shrink;
    _next_time -= shrink;
}

Clock::duration JitterBuffer::time_of(int64_t duration) const {
    return std::chrono::duration_cast<Clock::duration>(
            std::chrono::nanoseconds(duration * 1'000'000'000 / _clock_rate)
    );
}

int64_t JitterBuffer::duration_of(Clock::duration time) const {
    auto nanoseconds =
            std::chrono::duration_cast<std::chrono::nanoseconds>(time).count();
    return nanoseconds * _clock_rate / 1'000'000'000;
}

}  // namespace pipecat::smallwebrtc
