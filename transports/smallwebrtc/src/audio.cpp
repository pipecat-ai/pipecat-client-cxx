//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "audio.h"

#include <opus.h>

#include <stdexcept>
#include <string>

namespace pipecat::smallwebrtc {

namespace {

// Big enough for any packet Opus makes.
const size_t MAX_PACKET_SIZE = 1500;
// The longest audio an Opus packet can have.
const std::chrono::milliseconds MAX_PACKET_DURATION {120};

// `sample_rate` if Opus supports it, and 48 kHz otherwise.
uint32_t opus_sample_rate(uint32_t sample_rate) {
    switch (sample_rate) {
    case 8000:
    case 12000:
    case 16000:
    case 24000:
    case 48000:
        return sample_rate;
    default:
        return 48000;
    }
}

size_t frames_in(std::chrono::milliseconds duration, uint32_t sample_rate) {
    return static_cast<size_t>(duration.count()) * sample_rate / 1000;
}

void check(int error, const char* what) {
    if (error != OPUS_OK) {
        throw std::runtime_error(
                std::string("Unable to create the Opus ") + what + ": " +
                opus_strerror(error)
        );
    }
}

}  // namespace

//
// AudioEncoder
//

AudioEncoder::AudioEncoder(uint32_t sample_rate, uint32_t channels)
    : _sample_rate(sample_rate),
      _channels(channels),
      _opus_sample_rate(opus_sample_rate(sample_rate)),
      _resampler(_opus_sample_rate) {
    int error = OPUS_OK;
    _encoder = opus_encoder_create(
            static_cast<opus_int32>(_opus_sample_rate),
            1,
            OPUS_APPLICATION_VOIP,
            &error
    );
    check(error, "encoder");
}

AudioEncoder::~AudioEncoder() {
    opus_encoder_destroy(_encoder);
}

std::vector<std::vector<std::byte>>
AudioEncoder::encode(const int16_t* samples, size_t num_frames) {
    std::vector<int16_t> mono(num_frames);
    for (size_t i = 0; i < num_frames; ++i) {
        int32_t sum = 0;
        for (uint32_t channel = 0; channel < _channels; ++channel) {
            sum += samples[i * _channels + channel];
        }
        mono[i] = static_cast<int16_t>(sum / static_cast<int32_t>(_channels));
    }
    std::vector<int16_t> converted =
            _resampler.process(mono.data(), num_frames, 1, _sample_rate);
    _pending.insert(_pending.end(), converted.begin(), converted.end());

    size_t packet_frames = frames_in(PACKET_DURATION, _opus_sample_rate);
    std::vector<std::vector<std::byte>> packets;
    size_t encoded = 0;
    while (_pending.size() - encoded >= packet_frames) {
        std::vector<std::byte> packet(MAX_PACKET_SIZE);
        opus_int32 size = opus_encode(
                _encoder,
                _pending.data() + encoded,
                static_cast<int>(packet_frames),
                reinterpret_cast<unsigned char*>(packet.data()),
                static_cast<opus_int32>(packet.size())
        );
        encoded += packet_frames;
        // It only fails with wrong arguments.
        if (size > 0) {
            packet.resize(static_cast<size_t>(size));
            packets.push_back(std::move(packet));
        }
    }
    _pending.erase(_pending.begin(), _pending.begin() + encoded);
    return packets;
}

//
// AudioDecoder
//

AudioDecoder::AudioDecoder(uint32_t sample_rate)
    : _opus_sample_rate(opus_sample_rate(sample_rate)),
      _resampler(sample_rate) {
    int error = OPUS_OK;
    _decoder = opus_decoder_create(
            static_cast<opus_int32>(_opus_sample_rate), 1, &error
    );
    check(error, "decoder");
}

AudioDecoder::~AudioDecoder() {
    opus_decoder_destroy(_decoder);
}

std::vector<int16_t> AudioDecoder::decode(const std::byte* data, size_t size) {
    std::vector<int16_t> samples(
            frames_in(MAX_PACKET_DURATION, _opus_sample_rate)
    );
    int frames = opus_decode(
            _decoder,
            reinterpret_cast<const unsigned char*>(data),
            static_cast<opus_int32>(size),
            samples.data(),
            static_cast<int>(samples.size()),
            0
    );
    if (frames <= 0) {
        return {};
    }
    return _resampler.process(
            samples.data(), static_cast<size_t>(frames), 1, _opus_sample_rate
    );
}

}  // namespace pipecat::smallwebrtc
