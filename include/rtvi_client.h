//
// Copyright (c) 2024, Daily
//

#ifndef _RTVI_CLIENT_H
#define _RTVI_CLIENT_H

#include "rtvi_callbacks.h"
#include "rtvi_transport.h"

#include "json.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace rtvi {

struct RTVIClientEndpoints {
    std::string connect;
};

struct RTVIClientParams {
    std::string base_url;
    std::optional<std::string> api_key;
    std::optional<RTVIClientEndpoints> endpoints;
    nlohmann::json request;
    std::vector<std::string> headers;
};

struct RTVIClientOptions {
    RTVIClientParams params;
    RTVIEventCallbacks* callbacks;
};

class RTVIClient {
   public:
    explicit RTVIClient(
            const RTVIClientOptions& options,
            std::unique_ptr<RTVITransport> transport
    );

    virtual ~RTVIClient();

    void initialize();

    void connect();

    void disconnect();

    void send_action(const nlohmann::json& action);

    void send_action(const nlohmann::json& action, RTVIActionCallback callback);

    int32_t send_user_audio(const int16_t* frames, int num_frames);

   private:
    nlohmann::json connect_to_endpoint(
            const std::string& url,
            const std::optional<std::string> api_key,
            const nlohmann::json& body,
            const std::vector<std::string>& headers
    ) const;

    std::atomic<bool> _initialized;
    std::atomic<bool> _connected;

    std::mutex _mutex;
    RTVIClientOptions _options;
    std::unique_ptr<RTVITransport> _transport;
};

}  // namespace rtvi

#endif
