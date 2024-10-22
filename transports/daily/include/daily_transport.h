//
// Copyright (c) 2024, Daily
//

#ifndef DAILY_TRANSPORT_H
#define DAILY_TRANSPORT_H

#include "rtvi.h"

extern "C" {
#include "daily_core.h"
}

#include <atomic>
#include <future>
#include <mutex>

namespace rtvi {

struct DailyTransportParams {
    uint32_t user_audio_sample_rate;
    uint32_t user_audio_channels;
    uint32_t bot_audio_sample_rate;
    uint32_t bot_audio_channels;
};

class DailyTransport : public RTVITransport {
   public:
    explicit DailyTransport(
            const RTVIClientOptions& options,
            RTVITransportMessageObserver* message_observer
    );

    explicit DailyTransport(
            const RTVIClientOptions& options,
            const DailyTransportParams& params,
            RTVITransportMessageObserver* message_observer
    );

    virtual ~DailyTransport() override;

    void initialize() override;

    void connect(const nlohmann::json& info) override;

    void disconnect() override;

    void send_message(const nlohmann::json& message) override;

    int32_t send_user_audio(const int16_t* data, size_t num_frames) override;
    int32_t read_bot_audio(int16_t* data, size_t num_frames) override;

    // Internal usage only.
    WebrtcAudioDeviceModule* create_audio_device_module(
            WebrtcTaskQueueFactory* task_queue_factory
    );
    char* enumerate_devices();
    void* get_user_media(
            WebrtcPeerConnectionFactory* peer_connection_factory,
            WebrtcThread* webrtc_signaling_thread,
            WebrtcThread* webrtc_worker_thread,
            WebrtcThread* webrtc_network_thread,
            const char* constraints
    );

    void on_event(const nlohmann::json& event);

   private:
    uint64_t add_completion(std::promise<void> completion);
    void resolve_completion(uint64_t request_id);

    void send_message_thread();

    void on_participant_joined(const nlohmann::json& participant);
    void on_participant_updated(const nlohmann::json& participant);
    void on_participant_left(
            const nlohmann::json& participant,
            const std::string& reason
    );

   private:
    std::atomic<bool> _initialized;
    std::atomic<bool> _connected;

    RTVIClientOptions _options;
    DailyTransportParams _params;
    RTVITransportMessageObserver* _message_observer;

    DailyRawCallClient* _client;
    NativeDeviceManager* _device_manager;
    DailyVirtualSpeakerDevice* _speaker;
    DailyVirtualMicrophoneDevice* _microphone;

    // daily-core completions
    std::mutex _completions_mutex;
    std::atomic<uint64_t> _request_id;
    std::map<uint64_t, std::promise<void>> _completions;

    std::thread _msg_thread;
    RTVIQueue<nlohmann::json> _msg_queue;

    nlohmann::json _bot_participant;
};

}  // namespace rtvi

#endif
