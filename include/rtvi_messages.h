//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_MESSAGES_H
#define RTVI_MESSAGES_H

#include "rtvi_utils.h"

#include "json.hpp"

namespace rtvi {

struct UserTranscriptData {
    std::string text;
    bool final;
    std::string timestamp;
    std::string user_id;
};

struct BotLLMTextData {
    std::string text;
};

struct BotTTSTextData {
    std::string text;
};

struct RTVIMessage {
    static nlohmann::json message(const std::string& type) {
        return nlohmann::json {
                {"id", generate_random_id()},
                {"label", "rtvi-ai"},
                {"type", type},
        };
    }

    static nlohmann::json
    message(const std::string& type, const nlohmann::json& data) {
        return nlohmann::json {
                {"id", generate_random_id()},
                {"label", "rtvi-ai"},
                {"type", type},
                {"data", data},
        };
    }

    static nlohmann::json client_ready() { return message("client-ready"); }

    static nlohmann::json get_config() { return message("get-config"); }

    static nlohmann::json describe_config() {
        return message("describe-config");
    }

    static nlohmann::json describe_actions() {
        return message("describe-actions");
    }

    static nlohmann::json
    update_config(const nlohmann::json& config, bool interrupt = false) {
        return message(
                "update-config",
                nlohmann::json {{"config", config}, {"interrupt", interrupt}}
        );
    }

    static nlohmann::json action(
            const std::string& service,
            const std::string& action,
            const nlohmann::json& arguments
    ) {
        return message(
                "action",
                nlohmann::json {
                        {"service", service},
                        {"action", action},
                        {"arguments", arguments}
                }
        );
    }

    std::string id;
    std::string label;
    std::string type;
    nlohmann::json data;
};

}  // namespace rtvi

#endif
