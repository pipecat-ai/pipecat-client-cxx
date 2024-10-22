//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_TRANSPORT_H
#define RTVI_TRANSPORT_H

#include "rtvi_callbacks.h"

#include "json.hpp"

namespace rtvi {

class RTVITransportMessageObserver {
   public:
    virtual void on_transport_message(const nlohmann::json& message) = 0;
};

class RTVITransport {
   public:
    virtual ~RTVITransport() = default;

    virtual void initialize() = 0;

    virtual void connect(const nlohmann::json& info) = 0;

    virtual void disconnect() = 0;

    virtual void send_message(const nlohmann::json& message) = 0;

    virtual int32_t
    send_user_audio(const int16_t* frames, size_t num_frames) = 0;

    virtual int32_t read_bot_audio(int16_t* data, size_t num_frames) = 0;
};

}  // namespace rtvi

#endif
