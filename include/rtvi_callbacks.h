//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_CALLBACKS_H
#define RTVI_CALLBACKS_H

#include "rtvi_messages.h"

#include "json.hpp"

#include <functional>

namespace rtvi {

typedef std::function<void(const nlohmann::json& action)> RTVIActionCallback;

class RTVIEventCallbacks {
   public:
    virtual ~RTVIEventCallbacks() {}

    virtual void on_connected() {}
    virtual void on_disconnected() {}

    virtual void on_bot_connected(const nlohmann::json&) {}
    virtual void
    on_bot_disconnected(const nlohmann::json&, const std::string&) {}
    virtual void on_bot_ready() {}
    virtual void on_bot_started_speaking(const nlohmann::json&) {}
    virtual void on_bot_stopped_speaking(const nlohmann::json&) {}
    virtual void on_bot_transcript(const BotTranscriptData&) {}
    virtual void on_bot_tts_text(const BotTTSTextData&) {}
    virtual void on_bot_llm_text(const BotLLMTextData&) {}

    virtual void on_user_started_speaking() {}
    virtual void on_user_stopped_speaking() {}
    virtual void on_user_transcript(const UserTranscriptData&) {}

    virtual void on_message(const nlohmann::json&) {}
    virtual void on_message_error(const nlohmann::json&) {}
};

}  // namespace rtvi

#endif
