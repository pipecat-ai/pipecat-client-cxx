//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "audio_buffer.h"

#include <algorithm>

namespace pipecat::daily {

AudioBuffer::AudioBuffer(size_t channels, size_t max_frames)
    : _channels(std::max<size_t>(channels, 1)),
      _max_frames(std::max<size_t>(max_frames, 1)) {}

void AudioBuffer::open() {
    std::lock_guard<std::mutex> lock(_mutex);
    _samples.clear();
    _open = true;
}

void AudioBuffer::close() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _open = false;
        _samples.clear();
    }
    _cv.notify_all();
}

void AudioBuffer::write(
        const int16_t* samples,
        size_t num_frames,
        size_t channels
) {
    if (channels == 0) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_open) {
            return;
        }

        for (size_t frame = 0; frame < num_frames; ++frame) {
            const int16_t* in = samples + frame * channels;
            if (_channels == 1 && channels > 1) {
                // Mix down to mono.
                int32_t sum = 0;
                for (size_t c = 0; c < channels; ++c) {
                    sum += in[c];
                }
                _samples.push_back(static_cast<int16_t>(
                        sum / static_cast<int32_t>(channels)
                ));
            } else {
                // Copy the channels, repeating the last one if there are
                // fewer, e.g. mono to stereo.
                for (size_t c = 0; c < _channels; ++c) {
                    _samples.push_back(in[std::min(c, channels - 1)]);
                }
            }
        }

        size_t max_samples = _max_frames * _channels;
        if (_samples.size() > max_samples) {
            auto excess =
                    static_cast<std::ptrdiff_t>(_samples.size() - max_samples);
            _samples.erase(_samples.begin(), _samples.begin() + excess);
        }
    }
    _cv.notify_all();
}

size_t AudioBuffer::read(int16_t* samples, size_t num_frames) {
    // More than the buffer holds would never be available.
    num_frames = std::min(num_frames, _max_frames);
    if (num_frames == 0) {
        return 0;
    }

    size_t num_samples = num_frames * _channels;
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [&] { return !_open || _samples.size() >= num_samples; });
    if (!_open) {
        return 0;
    }

    auto end = _samples.begin() + static_cast<std::ptrdiff_t>(num_samples);
    std::copy(_samples.begin(), end, samples);
    _samples.erase(_samples.begin(), end);
    return num_frames;
}

}  // namespace pipecat::daily
