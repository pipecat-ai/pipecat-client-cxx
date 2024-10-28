//
// Copyright (c) 2024, Daily
//

#include "rtvi_llm_helper.h"

using namespace rtvi;

static const std::vector<std::string> SUPPORTED_MESSAGES = {
        "llm-function-call",
        "llm-function-call-start",
        "llm-json-completion"
};

RTVILLMHelper::RTVILLMHelper(const RTVILLMHelperOptions& options)
    : _options(options) {}

RTVILLMHelper::~RTVILLMHelper() {}

const std::vector<std::string>& RTVILLMHelper::supported_messages() {
    return SUPPORTED_MESSAGES;
}

void RTVILLMHelper::handle_message(
        RTVITransport* transport,
        const nlohmann::json& message
) {
    auto type = message["type"].get<std::string>();
    switch (hash(type.c_str())) {
    case hash("llm-function-call"): {
        if (_options.callbacks) {
            auto function_name =
                    message["data"]["function_name"].get<std::string>();
            auto tool_call_id =
                    message["data"]["tool_call_id"].get<std::string>();
            auto args = message["data"]["args"];

            auto function_call_data = LLMFunctionCallData {
                    .function_name = function_name,
                    .tool_call_id = tool_call_id,
                    .args = args,
            };

            std::optional<nlohmann::json> result =
                    _options.callbacks->on_function_call(function_call_data);
            if (result) {
                nlohmann::json data = {
                        {"function_name", function_name},
                        {"tool_call_id", tool_call_id},
                        {"arguments", args},
                        {"result", *result},
                };
                auto message =
                        RTVIMessage::message("llm-function-call-result", data);
                transport->send_message(message);
            } else {
                nlohmann::json data = {
                        {"function_name", function_name},
                        {"tool_call_id", tool_call_id},
                        {"arguments", args},
                        {"result", ""},
                };
                auto message =
                        RTVIMessage::message("llm-function-call-result", data);
                transport->send_message(message);
            }
        }
        break;
    }
    case hash("llm-function-call-start"): {
        if (_options.callbacks) {
            auto function_name =
                    message["data"]["function_name"].get<std::string>();
            _options.callbacks->on_function_call_start(function_name);
        }
        break;
    }
    }
}
