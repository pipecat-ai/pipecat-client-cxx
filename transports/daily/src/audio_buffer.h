//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_DAILY_AUDIO_BUFFER_H
#define PIPECAT_DAILY_AUDIO_BUFFER_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace pipecat::daily {

// 16-bit PCM audio received from the bot until the app reads it. Thread-safe.
class AudioBuffer {
   public:
    // Holds up to `max_frames` frames of `channels` channels. It starts
    // closed.
    AudioBuffer(size_t channels, size_t max_frames);

    // Starts accepting audio, and drops any left from before.
    void open();

    // Stops accepting audio, and wakes up readers.
    void close();

    // Adds `num_frames` frames of `channels` channels, converted to the
    // buffer's channels. Drops the oldest audio if the buffer is full, e.g.
    // because the app doesn't read it. Ignored while closed.
    void write(const int16_t* samples, size_t num_frames, size_t channels);

    // Waits until `num_frames` frames can be read, or the buffer is closed.
    // Returns the number of frames read, which is 0 if it's closed.
    size_t read(int16_t* samples, size_t num_frames);

   private:
    const size_t _channels;
    const size_t _max_frames;

    // Guarded by _mutex. _cv wakes up readers when audio arrives or the
    // buffer closes.
    std::mutex _mutex;
    std::condition_variable _cv;
    // Interleaved samples, oldest first.
    std::deque<int16_t> _samples;
    bool _open = false;
};

}  // namespace pipecat::daily

#endif
