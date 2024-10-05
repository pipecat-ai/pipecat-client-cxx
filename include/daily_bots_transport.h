//
// Copyright (c) 2024, Daily
//

#ifndef _DAILY_BOTS_TRANSPORT_H
#define _DAILY_BOTS_TRANSPORT_H

#include "rtvi.h"

extern "C" {
#include "daily_core.h"
}

#include <atomic>
#include <future>
#include <mutex>

namespace daily {

class DailyBotsTransport : public rtvi::RTVITransport {
   public:
    explicit DailyBotsTransport(const rtvi::RTVIClientOptions& options);

    virtual ~DailyBotsTransport();

    void initialize() override;

    void connect(const nlohmann::json& info) override;

    void disconnect() override;

    void send_message(const nlohmann::json& message) override;

    void send_action(const nlohmann::json& action) override;

    void send_action(
            const nlohmann::json& action,
            rtvi::RTVIActionCallback callback
    ) override;

    int32_t send_user_audio(const int16_t* data, size_t num_frames) override;

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
    void on_audio_data(const struct NativeAudioData* audio_data);

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
    void on_rtvi_message(const nlohmann::json& message);
    void on_rtvi_action_response(const nlohmann::json& response);

    std::atomic<bool> _initialized;
    std::atomic<bool> _connected;

    rtvi::RTVIClientOptions _options;

    DailyRawCallClient* _client;
    NativeDeviceManager* _device_manager;
    DailyVirtualMicrophoneDevice* _microphone;

    // daily-core completions
    std::mutex _completions_mutex;
    std::atomic<uint64_t> _request_id;
    std::map<uint64_t, std::promise<void>> _completions;

    // RTVI action-response
    std::mutex _actions_mutex;
    std::map<std::string, rtvi::RTVIActionCallback> _action_callbacks;

    std::thread _msg_thread;
    rtvi::RTVIQueue<nlohmann::json> _msg_queue;

    nlohmann::json _bot_participant;
};

}  // namespace daily

#endif
