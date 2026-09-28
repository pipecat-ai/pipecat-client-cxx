//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "audio.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(PaError error) {
    if (error != paNoError) {
        throw std::runtime_error(
                std::string("PortAudio: ") + Pa_GetErrorText(error)
        );
    }
}

}  // namespace

Audio::Audio(pipecat::PipecatClient& client, uint32_t sample_rate)
    : _client(client), _sample_rate(sample_rate) {
    check(Pa_Initialize());
}

Audio::~Audio() {
    stop();
    Pa_Terminate();
}

void Audio::start() {
    // 10 ms of audio at a time.
    unsigned long frames_per_buffer = _sample_rate / 100;

    check(Pa_OpenDefaultStream(
            &_microphone,
            1,
            0,
            paInt16,
            _sample_rate,
            frames_per_buffer,
            &Audio::on_microphone,
            this
    ));
    check(Pa_OpenDefaultStream(
            &_speaker,
            0,
            1,
            paInt16,
            _sample_rate,
            frames_per_buffer,
            nullptr,
            nullptr
    ));

    check(Pa_StartStream(_microphone));
    check(Pa_StartStream(_speaker));
    _speaker_thread = std::thread([this] { play_bot_audio(); });
}

void Audio::stop() {
    if (_speaker_thread.joinable()) {
        _speaker_thread.join();
    }
    for (PaStream** stream: {&_microphone, &_speaker}) {
        if (*stream != nullptr) {
            Pa_StopStream(*stream);
            Pa_CloseStream(*stream);
            *stream = nullptr;
        }
    }
}

int Audio::on_microphone(
        const void* input,
        void* /* output */,
        unsigned long num_frames,
        const PaStreamCallbackTimeInfo* /* time_info */,
        PaStreamCallbackFlags /* flags */,
        void* user_data
) {
    auto* audio = static_cast<Audio*>(user_data);
    audio->_client.send_user_audio(
            static_cast<const int16_t*>(input), num_frames
    );
    return paContinue;
}

void Audio::play_bot_audio() {
    std::vector<int16_t> frames(_sample_rate / 100);
    // Waits until the bot has audio to play, and returns 0 once the client
    // disconnects.
    while (int32_t read =
                   _client.read_bot_audio(frames.data(), frames.size())) {
        Pa_WriteStream(_speaker, frames.data(), read);
    }
}
