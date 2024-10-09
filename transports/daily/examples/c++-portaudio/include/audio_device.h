#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "rtvi.h"

#include <portaudio.h>

#include <atomic>
#include <memory>
#include <thread>

class AudioInput {
   public:
    explicit AudioInput(rtvi::RTVIClient* client, const uint32_t sample_rate);

    virtual ~AudioInput();

    virtual void start();

    void stop();

   private:
    static int pa_audio_input_callback(
            const void* input_buffer,
            void* output_buffer,
            unsigned long num_frames,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags flags,
            void* user_data
    );

    int
    audio_input_callback(const void* input_buffer, unsigned long num_frames);

   private:
    std::atomic<bool> _recording;
    rtvi::RTVIClient* _client;
    uint32_t _sample_rate;
    PaStream* _input_stream;
};

class AudioOutput {
   public:
    explicit AudioOutput(rtvi::RTVIClient* client, const uint32_t sample_rate);

    virtual ~AudioOutput();

    virtual void start();

    virtual void stop();

   private:
    void read_thread_handler();

    void append_audio(const int16_t* frames, const size_t num_frames);

    static int pa_audio_output_callback(
            const void* input_buffer,
            void* output_buffer,
            unsigned long num_frames,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags flags,
            void* user_data
    );

    int audio_output_callback(void* output_buffer, unsigned long num_frames);

   private:
    bool _started;
    bool _playing;
    rtvi::RTVIClient* _client;
    uint32_t _sample_rate;
    PaStream* _output_stream;
    std::thread _read_thread;
    std::mutex _buffer_mutex;
    std::deque<int16_t> _buffer;
};

class AudioDevice {
   public:
    AudioDevice(rtvi::RTVIClient* client);

    virtual ~AudioDevice();

    virtual void start();

    virtual void stop();

   private:
    std::unique_ptr<AudioInput> _input;
    std::unique_ptr<AudioOutput> _output;
};

#endif
