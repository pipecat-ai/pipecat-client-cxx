//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "context.h"

extern "C" {
#include "daily_core.h"
#include "daily_core_version.h"
}

#include <cstddef>
#include <mutex>

namespace pipecat::daily {

namespace {

// NOTE: Do not modify. This is how Daily recognizes a known client library.
const char* DAILY_LIBRARY = "daily-core-sdk";

std::mutex context_mutex;
size_t context_refs = 0;

DailyDeviceManager* device_manager(DailyRawWebRtcContextDelegate* delegate) {
    return static_cast<DailyDeviceManager*>(delegate);
}

WebrtcAudioDeviceModule* create_audio_device_module(
        DailyRawWebRtcContextDelegate* delegate,
        WebrtcTaskQueueFactory* task_queue_factory
) {
    return daily_core_context_create_audio_device_module(
            device_manager(delegate), task_queue_factory
    );
}

void* get_user_media(
        DailyRawWebRtcContextDelegate* delegate,
        WebrtcPeerConnectionFactory* peer_connection_factory,
        WebrtcThread* signaling_thread,
        WebrtcThread* worker_thread,
        WebrtcThread* network_thread,
        const char* constraints
) {
    return daily_core_context_device_manager_get_user_media(
            device_manager(delegate),
            peer_connection_factory,
            signaling_thread,
            worker_thread,
            network_thread,
            constraints
    );
}

char* get_enumerated_devices(DailyRawWebRtcContextDelegate* delegate) {
    return daily_core_context_device_manager_enumerated_devices(
            device_manager(delegate)
    );
}

const char* get_audio_device(DailyRawWebRtcContextDelegate*) {
    return "";
}

void set_audio_device(DailyRawWebRtcContextDelegate*, const char*) {}

}  // namespace

ContextRef::ContextRef() {
    std::lock_guard<std::mutex> lock(context_mutex);
    if (context_refs++ > 0) {
        return;
    }

    daily_core_set_log_level(DailyLogLevel_Off);

    DailyContextDelegate driver {};

    DailyWebRtcContextDelegate webrtc {};
    webrtc.ptr = daily_core_context_create_device_manager();
    webrtc.fns.get_user_media = get_user_media;
    webrtc.fns.get_enumerated_devices = get_enumerated_devices;
    webrtc.fns.create_audio_device_module = create_audio_device_module;
    webrtc.fns.get_audio_device = get_audio_device;
    webrtc.fns.set_audio_device = set_audio_device;

    DailyAboutClient about {};
    about.library = DAILY_LIBRARY;
    about.version = DAILY_CORE_VERSION;

    daily_core_context_create(driver, webrtc, about);
}

ContextRef::~ContextRef() {
    std::lock_guard<std::mutex> lock(context_mutex);
    if (--context_refs == 0) {
        daily_core_context_destroy();
    }
}

}  // namespace pipecat::daily
