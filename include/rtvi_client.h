//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_CLIENT_H
#define RTVI_CLIENT_H

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
    RTVIClientEndpoints endpoints;
    nlohmann::json request;
    std::vector<std::string> headers;
};

struct RTVIClientOptions {
    RTVIClientParams params;
    RTVIEventCallbacks* callbacks;
};

class RTVIClient : public RTVITransportMessageObserver {
   public:
    explicit RTVIClient(
            const RTVIClientOptions& options,
            std::unique_ptr<RTVITransport> transport
    );

    virtual ~RTVIClient();

    virtual void initialize();

    virtual void connect();

    virtual void disconnect();

    virtual void send_action(const nlohmann::json& action);

    virtual void
    send_action(const nlohmann::json& action, RTVIActionCallback callback);

    virtual int32_t send_user_audio(const int16_t* frames, size_t num_frames);

    virtual int32_t read_bot_audio(int16_t* frames, size_t num_frames);

    // RTVITransportMessageObserver
    void on_transport_message(const nlohmann::json& message);

   private:
    nlohmann::json connect_to_endpoint(
            const std::string& url,
            const nlohmann::json& body,
            const std::vector<std::string>& headers
    ) const;
    void on_action_response(const nlohmann::json& response);

   private:
    std::atomic<bool> _initialized;
    std::atomic<bool> _connected;

    std::mutex _mutex;
    RTVIClientOptions _options;
    std::unique_ptr<RTVITransport> _transport;

    // RTVI action-response
    std::mutex _actions_mutex;
    std::map<std::string, RTVIActionCallback> _action_callbacks;
};

}  // namespace rtvi

#endif
