//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "resampler.h"

#include <speex/speex_resampler.h>

namespace pipecat::smallwebrtc {

Resampler::Resampler(uint32_t sample_rate) : _sample_rate(sample_rate) {}

Resampler::~Resampler() {
    if (_state != nullptr) {
        speex_resampler_destroy(_state);
    }
}

std::vector<int16_t> Resampler::process(
        const int16_t* samples,
        size_t num_frames,
        uint32_t channels,
        uint32_t sample_rate
) {
    if (sample_rate == _sample_rate) {
        return {samples, samples + num_frames * channels};
    }

    if (_state == nullptr || channels != _channels ||
        sample_rate != _input_sample_rate) {
        if (_state != nullptr) {
            speex_resampler_destroy(_state);
        }
        _state = speex_resampler_init(
                channels,
                sample_rate,
                _sample_rate,
                SPEEX_RESAMPLER_QUALITY_DEFAULT,
                nullptr
        );
        _channels = channels;
        _input_sample_rate = sample_rate;
    }

    // Room for the converted frames, plus one for rounding.
    auto in_len = static_cast<spx_uint32_t>(num_frames);
    auto out_len = static_cast<spx_uint32_t>(
            uint64_t {num_frames} * _sample_rate / sample_rate + 1
    );
    std::vector<int16_t> out(size_t {out_len} * channels);
    speex_resampler_process_interleaved_int(
            _state, samples, &in_len, out.data(), &out_len
    );
    out.resize(size_t {out_len} * channels);
    return out;
}

void Resampler::reset() {
    if (_state != nullptr) {
        speex_resampler_reset_mem(_state);
    }
}

}  // namespace pipecat::smallwebrtc
