//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_CLIENT_H
#define PIPECAT_CLIENT_H

#include "pipecat/rtvi/messages.h"
#include "pipecat/transport.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace pipecat {

// Receives client events. Override the ones you need.
//
// All callbacks run on the client's own event thread, one at a time and in
// the order events happened. It's safe to call any PipecatClient method from
// them, including disconnect(). Events wait while a callback runs, so avoid
// long work. Callbacks must not throw (exceptions are ignored), and must not
// destroy the client.
class PipecatClientCallbacks {
   public:
    virtual ~PipecatClientCallbacks() = default;

    // Connection.
    virtual void on_transport_state_changed(TransportState /* state */) {}
    virtual void on_connected() {}
    virtual void on_disconnected() {}
    // Errors from the bot, and client errors such as malformed messages.
    virtual void on_error(const rtvi::ErrorData& /* error */) {}

    // Bot and participants.
    virtual void on_bot_started(const nlohmann::json& /* response */) {}
    virtual void on_bot_connected(const Participant& /* bot */) {}
    virtual void on_bot_ready(const rtvi::BotReadyData& /* data */) {}
    virtual void on_bot_disconnected(const Participant& /* bot */) {}
    virtual void on_participant_joined(const Participant& /* participant */) {}
    virtual void on_participant_left(const Participant& /* participant */) {}

    // Messages.
    virtual void on_server_message(const nlohmann::json& /* data */) {}
    // The bot couldn't handle a message sent by the client.
    virtual void on_message_error(const rtvi::ErrorData& /* error */) {}
    virtual void on_metrics(const rtvi::MetricsData& /* data */) {}
    // A message this client doesn't handle, e.g. a newer message type.
    virtual void on_unhandled_message(const rtvi::Message& /* message */) {}

    // Speaking.
    virtual void on_user_started_speaking() {}
    virtual void on_user_stopped_speaking() {}
    virtual void on_bot_started_speaking() {}
    virtual void on_bot_stopped_speaking() {}
    // The bot is ignoring user audio (e.g. while it's speaking). Keep sending
    // audio as usual.
    virtual void on_user_mute_started() {}
    virtual void on_user_mute_stopped() {}

    // Transcription and output.
    virtual void on_user_transcript(const rtvi::TranscriptData& /* data */) {}
    virtual void on_user_llm_text(const rtvi::UserLLMTextData& /* data */) {}
    virtual void on_bot_output(const rtvi::BotOutputData& /* data */) {}

    // LLM.
    virtual void on_bot_llm_started() {}
    virtual void on_bot_llm_stopped() {}
    virtual void on_bot_llm_text(const rtvi::BotLLMTextData& /* data */) {}
    virtual void on_bot_llm_search_response(
            const rtvi::BotLLMSearchResponseData& /* data */
    ) {}

    // Function calls.
    virtual void on_llm_function_call_started(
            const rtvi::LLMFunctionCallStartedData& /* data */
    ) {}
    virtual void on_llm_function_call_in_progress(
            const rtvi::LLMFunctionCallInProgressData& /* data */
    ) {}
    virtual void on_llm_function_call_stopped(
            const rtvi::LLMFunctionCallStoppedData& /* data */
    ) {}

    // TTS.
    virtual void on_bot_tts_started() {}
    virtual void on_bot_tts_stopped() {}
    virtual void on_bot_tts_text(const rtvi::BotTTSTextData& /* data */) {}
};

// A request to a bot start endpoint, e.g. Pipecat Cloud or your own server.
struct APIRequest {
    std::string endpoint;
    std::map<std::string, std::string> headers;
    // JSON body of the POST request. Null sends no body.
    nlohmann::json request_data;
    // Zero means no timeout.
    std::chrono::milliseconds timeout {0};
};

// Result of a send_client_request() with a callback.
struct ClientResponse {
    // What the bot answered with. Null if the request failed.
    nlohmann::json data;
    // Why the request failed: the bot answered with an error, it didn't
    // answer in time, or the client disconnected first.
    std::optional<std::string> error;
    // Whether the request failed because the bot didn't answer in time.
    bool timed_out = false;
};

using ClientResponseCallback = std::function<void(const ClientResponse&)>;

struct PipecatClientOptions {
    // Required.
    std::unique_ptr<Transport> transport;
    // Optional. Must outlive the client.
    PipecatClientCallbacks* callbacks = nullptr;
    // Disconnect when the bot leaves.
    bool disconnect_on_bot_disconnect = true;
    // How long connect() waits for the bot to be ready. Zero waits forever.
    std::chrono::milliseconds connect_timeout = std::chrono::seconds(30);
    // Sent to the bot in client-ready. Add app details to platform_details.
    rtvi::AboutClientData about = rtvi::default_about_client();
};

// Connects to a Pipecat bot through a transport.
//
// Methods are thread-safe. start_bot(), connect() and disconnect() block the
// calling thread; everything else returns right away.
//
// Methods that send messages need the bot to be ready (throw
// BotNotReadyError otherwise) and throw MessageTooLargeError if the message
// is larger than the transport allows.
class PipecatClient {
   public:
    explicit PipecatClient(PipecatClientOptions options);

    // Disconnects if needed and delivers pending callbacks before returning.
    ~PipecatClient();

    PipecatClient(const PipecatClient&) = delete;
    PipecatClient& operator=(const PipecatClient&) = delete;

    // Prepares the transport (e.g. audio devices). Optional, start_bot() and
    // connect() do it if needed.
    void initialize();

    // POSTs to a bot start endpoint and returns its JSON response, usually
    // the transport parameters for connect(). Throws BotAlreadyStartedError
    // or StartBotError.
    nlohmann::json start_bot(const APIRequest& request);

    // Connects the transport and waits until the bot is ready. Throws
    // BotAlreadyStartedError, ConnectionTimeoutError or a transport error,
    // and disconnects on failure.
    rtvi::BotReadyData connect(
            const nlohmann::json& transport_params = nullptr
    );

    // start_bot() and then connect() with its response.
    rtvi::BotReadyData start_bot_and_connect(const APIRequest& request);

    // Disconnects, cancelling a start_bot() or connect() in progress.
    void disconnect();

    TransportState state() const;

    // Whether the transport is connected (Connected or Ready).
    bool connected() const;

    // The transport from the options, e.g. to call transport-specific
    // methods. Use the client, not the transport, to connect and disconnect.
    Transport& transport();

    // Sends 16-bit PCM user audio. Returns the number of frames sent, 0 if not
    // connected.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames);

    // Reads 16-bit PCM bot audio. Returns the number of frames read, 0 if not
    // connected.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames);

    // Sends text to the bot's LLM, as if the user had said it.
    void send_text(
            const std::string& content,
            const rtvi::SendTextOptions& options = {}
    );

    // Sends an app-defined message to the bot.
    void send_client_message(
            const std::string& type,
            const nlohmann::json& data = nullptr
    );

    // Sends an app-defined message and calls `callback` with the bot's
    // answer, on the event thread. Zero timeout waits forever.
    void send_client_request(
            const std::string& type,
            const nlohmann::json& data,
            ClientResponseCallback callback,
            std::chrono::milliseconds timeout = std::chrono::seconds(10)
    );

    // Sends an app-defined message and returns the bot's answer. get() throws
    // MessageError or RequestTimeoutError if it fails. Safe to wait on from a
    // callback. Zero timeout waits forever.
    std::future<nlohmann::json> send_client_request(
            const std::string& type,
            const nlohmann::json& data = nullptr,
            std::chrono::milliseconds timeout = std::chrono::seconds(10)
    );

    // Asks the bot to leave, keeping the transport connected.
    void disconnect_bot();

    // Sends one or more DTMF keys (0-9, * and #), e.g. "123#". Throws
    // PipecatError for other characters and UnsupportedFeatureError if the
    // bot is older than RTVI protocol 2.0.0.
    void send_dtmf(const std::string& buttons);

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
