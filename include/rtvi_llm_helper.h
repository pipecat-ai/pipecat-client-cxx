//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_LLM_HELPER_H
#define RTVI_LLM_HELPER_H

#include "rtvi_helper.h"

#include <optional>

namespace rtvi {

struct LLMFunctionCallData {
    std::string function_name;
    std::string tool_call_id;
    nlohmann::json args;
};

class RTVILLMHelperCallbacks {
   public:
    virtual ~RTVILLMHelperCallbacks() {}

    virtual std::optional<nlohmann::json>
    on_function_call(const LLMFunctionCallData&) {
        return std::nullopt;
    }
    virtual void on_function_call_start(const std::string&) {}
};

struct RTVILLMHelperOptions {
    RTVILLMHelperCallbacks* callbacks;
};

class RTVILLMHelper : public RTVIHelper {
   public:
    explicit RTVILLMHelper(const RTVILLMHelperOptions& options);

    virtual ~RTVILLMHelper();

    virtual const std::vector<std::string>& supported_messages();

    virtual void
    handle_message(RTVITransport* transport, const nlohmann::json& message);

   private:
    RTVILLMHelperOptions _options;
};

}  // namespace rtvi

#endif
