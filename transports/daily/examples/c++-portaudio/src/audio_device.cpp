//
// Copyright (c) 2024, Daily
//

#include "audio_device.h"

AudioInput::AudioInput(rtvi::RTVIClient* client, const uint32_t sample_rate)
    : _recording(false),
      _client(client),
      _sample_rate(sample_rate),
      _input_stream(nullptr) {}

AudioInput::~AudioInput() {
    if (_recording) {
        stop();
    }
}

void AudioInput::start() {
    PaError err = Pa_OpenDefaultStream(
            &_input_stream,
            1,
            0,
            paInt16,
            _sample_rate,
            paFramesPerBufferUnspecified,
            &pa_audio_input_callback,
            this
    );

    if (err != paNoError) {
        throw std::runtime_error(
                "PortAudio error: " + std::string(Pa_GetErrorText(err))
        );
    }

    err = Pa_StartStream(_input_stream);

    if (err != paNoError) {
        Pa_CloseStream(_input_stream);
        throw std::runtime_error(
                "PortAudio error: " + std::string(Pa_GetErrorText(err))
        );
    }

    _recording = true;
}

void AudioInput::stop() {
    _recording = false;
    Pa_StopStream(_input_stream);
    Pa_CloseStream(_input_stream);
}

int AudioInput::pa_audio_input_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long num_frames,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* user_data
) {
    return static_cast<AudioInput*>(user_data)->audio_input_callback(
            input_buffer, num_frames
    );
}

int AudioInput::audio_input_callback(
        const void* input_buffer,
        unsigned long num_frames
) {
    _client->send_user_audio(
            static_cast<const int16_t*>(input_buffer), num_frames
    );

    return paContinue;
}

AudioOutput::AudioOutput(rtvi::RTVIClient* client, const uint32_t sample_rate)
    : _started(false),
      _playing(false),
      _client(client),
      _sample_rate(sample_rate),
      _output_stream(nullptr),
      _buffer_mutex(),
      _buffer() {}

AudioOutput::~AudioOutput() {
    if (_started) {
        stop();
    }
}

void AudioOutput::start() {
    PaError err = Pa_OpenDefaultStream(
            &_output_stream,
            0,
            1,
            paInt16,
            _sample_rate,
            paFramesPerBufferUnspecified,
            &pa_audio_output_callback,
            this
    );

    if (err != paNoError) {
        throw std::runtime_error(
                "PortAudio error: " + std::string(Pa_GetErrorText(err))
        );
    }

    err = Pa_StartStream(_output_stream);

    if (err != paNoError) {
        Pa_CloseStream(_output_stream);
        throw std::runtime_error(
                "PortAudio error: " + std::string(Pa_GetErrorText(err))
        );
    }
    _started = true;
    _read_thread = std::thread(&AudioOutput::read_thread_handler, this);
}

void AudioOutput::stop() {
    _started = false;
    _read_thread.join();
    Pa_StopStream(_output_stream);
    Pa_CloseStream(_output_stream);
}

void AudioOutput::read_thread_handler() {
    size_t num_frames = 160;
    int16_t* frames = (int16_t*)malloc(num_frames * sizeof(int16_t));
    while (_started) {
        _client->read_bot_audio(frames, num_frames);
        append_audio(frames, num_frames);
    }
}

void AudioOutput::append_audio(const int16_t* frames, const size_t num_frames) {
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    _buffer.insert(_buffer.end(), frames, frames + num_frames);
}

int AudioOutput::pa_audio_output_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long num_frames,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* user_data
) {
    return static_cast<AudioOutput*>(user_data)->audio_output_callback(
            output_buffer, num_frames
    );
}

int AudioOutput::audio_output_callback(
        void* output_buffer,
        unsigned long num_frames
) {
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    int16_t* output_buffer_i16 = static_cast<int16_t*>(output_buffer);

    if (!_playing) {
        const size_t MIN_STARTING_FRAMES =
                std::max<size_t>(num_frames * 2, 160 * 2);

        if (_buffer.size() >= MIN_STARTING_FRAMES) {
            _playing = true;
        } else {
            memset(output_buffer_i16, 0, num_frames * 2);
            return paContinue;
        }
    }

    const size_t frames_remaining = _buffer.size();

    const size_t frames_to_copy =
            std::min<size_t>(num_frames, frames_remaining);

    std::copy(
            _buffer.begin(), _buffer.begin() + frames_to_copy, output_buffer_i16
    );

    _buffer.erase(_buffer.begin(), _buffer.begin() + frames_to_copy);

    if (frames_to_copy < num_frames) {
        memset(output_buffer_i16 + frames_to_copy,
               0,
               (num_frames - frames_to_copy) * 2);
        _playing = false;
    }

    return paContinue;
}

AudioDevice::AudioDevice(rtvi::RTVIClient* client) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error(
                "PortAudio error: " + std::string(Pa_GetErrorText(err))
        );
    }

    _input = std::make_unique<AudioInput>(client, 16000);
    _output = std::make_unique<AudioOutput>(client, 16000);
}

AudioDevice::~AudioDevice() {
    Pa_Terminate();
}

void AudioDevice::start() {
    _input->start();
    _output->start();
}

void AudioDevice::stop() {
    _input->stop();
    _output->stop();
}
