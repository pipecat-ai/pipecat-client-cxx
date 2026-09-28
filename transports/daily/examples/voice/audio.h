//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef AUDIO_H
#define AUDIO_H

#include <pipecat/client.h>

#include <portaudio.h>

#include <cstdint>
#include <thread>

// Sends the microphone to the bot and plays the bot on the speakers, as 16-bit
// mono PCM, with the default PortAudio devices.
class Audio {
   public:
    Audio(pipecat::PipecatClient& client, uint32_t sample_rate);
    ~Audio();

    // Starts sending and playing audio. Call it once connected.
    void start();
    // Stops the audio. Disconnect the client first, which ends the bot's
    // audio.
    void stop();

   private:
    static int on_microphone(
            const void* input,
            void* output,
            unsigned long num_frames,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags flags,
            void* user_data
    );

    void play_bot_audio();

    pipecat::PipecatClient& _client;
    uint32_t _sample_rate;
    PaStream* _microphone = nullptr;
    PaStream* _speaker = nullptr;
    std::thread _speaker_thread;
};

#endif
