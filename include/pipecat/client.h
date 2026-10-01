//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// The client, its options and its callbacks.

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

/// The Pipecat C++ client SDK.
namespace pipecat {

/// Sends the result of a function call to the bot.
///
/// Call it once, right away or later, from any thread. It does nothing after
/// the client disconnects. A null result is sent as an empty object.
using FunctionCallResultCallback = std::function<void(nlohmann::json result)>;

/// Receives events from the client. Override the ones you need.
///
/// Callbacks run one at a time and in order, on the client's own thread. You
/// can call any client method from them, including disconnect(). Keep them
/// short, because other events wait while one runs. Callbacks must not throw,
/// and must not destroy the client.
class PipecatClientCallbacks {
   public:
    virtual ~PipecatClientCallbacks() = default;

    /// @name Connection
    /// @{

    /// The connection state changed.
    virtual void on_transport_state_changed(TransportState /* state */) {}

    /// The client connected. The bot isn't ready yet, see on_bot_ready().
    virtual void on_connected() {}

    /// The client disconnected.
    virtual void on_disconnected() {}

    /// Something went wrong in the bot or the client, e.g. the bot sent an
    /// invalid message.
    virtual void on_error(const rtvi::ErrorData& /* error */) {}

    /// @}

    /// @name Bot and participants
    /// @{

    /// The bot was started. `response` is what the start endpoint answered.
    virtual void on_bot_started(const nlohmann::json& /* response */) {}

    /// The bot joined the session.
    virtual void on_bot_connected(const Participant& /* bot */) {}

    /// The bot is ready to talk.
    virtual void on_bot_ready(const rtvi::BotReadyData& /* data */) {}

    /// The bot left the session.
    virtual void on_bot_disconnected(const Participant& /* bot */) {}

    /// Someone other than the bot joined the session.
    virtual void on_participant_joined(const Participant& /* participant */) {}

    /// Someone other than the bot left the session.
    virtual void on_participant_left(const Participant& /* participant */) {}

    /// @}

    /// @name Messages
    /// @{

    /// The bot sent an app-defined message.
    virtual void on_server_message(const nlohmann::json& /* data */) {}

    /// The bot couldn't handle a message from the client.
    virtual void on_message_error(const rtvi::ErrorData& /* error */) {}

    /// The bot sent performance metrics.
    virtual void on_metrics(const rtvi::MetricsData& /* data */) {}

    /// The bot sent a message the client doesn't handle, e.g. a newer type of
    /// message.
    virtual void on_unhandled_message(const rtvi::Message& /* message */) {}

    /// @}

    /// @name Speaking
    /// @{

    /// The user started speaking.
    virtual void on_user_started_speaking() {}

    /// The user stopped speaking.
    virtual void on_user_stopped_speaking() {}

    /// The bot started speaking.
    virtual void on_bot_started_speaking() {}

    /// The bot stopped speaking.
    virtual void on_bot_stopped_speaking() {}

    /// The bot was interrupted, e.g. because the user spoke over it. Drop any
    /// bot audio you haven't played yet.
    virtual void on_bot_interrupted() {}

    /// The bot started ignoring user audio, e.g. while it speaks. Keep sending
    /// audio as usual.
    virtual void on_user_mute_started() {}

    /// The bot stopped ignoring user audio.
    virtual void on_user_mute_stopped() {}

    /// @}

    /// @name Transcription and output
    /// @{

    /// What the user said. It can arrive several times, until it's final.
    virtual void on_user_transcript(const rtvi::TranscriptData& /* data */) {}

    /// User text was sent to the bot's LLM.
    virtual void on_user_llm_text(const rtvi::UserLLMTextData& /* data */) {}

    /// What the bot says or outputs.
    virtual void on_bot_output(const rtvi::BotOutputData& /* data */) {}

    /// @}

    /// @name LLM
    /// @{

    /// The bot's LLM started responding.
    virtual void on_bot_llm_started() {}

    /// The bot's LLM finished responding.
    virtual void on_bot_llm_stopped() {}

    /// Text from the bot's LLM, as it streams.
    virtual void on_bot_llm_text(const rtvi::BotLLMTextData& /* data */) {}

    /// Search results used by the bot's LLM.
    virtual void on_bot_llm_search_response(
            const rtvi::BotLLMSearchResponseData& /* data */
    ) {}

    /// @}

    /// @name Function calls
    /// @{

    /// The bot's LLM started a function call.
    virtual void on_llm_function_call_started(
            const rtvi::LLMFunctionCallStartedData& /* data */
    ) {}

    /// The bot's LLM called a function. If your app runs it, call `respond`
    /// with the result. For slow work, call it later, e.g. from another
    /// thread, so other events don't wait. Calls to functions the bot runs
    /// itself also arrive here: don't call `respond` for them.
    virtual void on_llm_function_call_in_progress(
            const rtvi::LLMFunctionCallInProgressData& /* data */,
            FunctionCallResultCallback /* respond */
    ) {}

    /// A function call finished or was cancelled.
    virtual void on_llm_function_call_stopped(
            const rtvi::LLMFunctionCallStoppedData& /* data */
    ) {}

    /// @}

    /// @name Text-to-speech
    /// @{

    /// The bot's text-to-speech started.
    virtual void on_bot_tts_started() {}

    /// The bot's text-to-speech stopped.
    virtual void on_bot_tts_stopped() {}

    /// Text the bot is speaking.
    virtual void on_bot_tts_text(const rtvi::BotTTSTextData& /* data */) {}

    /// @}
};

/// A request to start a bot, e.g. on Pipecat Cloud or your own server. See
/// PipecatClient::start_bot().
struct APIRequest {
    /// URL of the start endpoint.
    std::string endpoint;
    /// HTTP headers, e.g. for authentication.
    std::map<std::string, std::string> headers;
    /// JSON body of the request. Null sends no body.
    nlohmann::json request_data;
    /// How long to wait for an answer. Zero waits forever.
    std::chrono::milliseconds timeout {0};
};

/// The bot's answer to a request.
struct ClientResponse {
    /// What the bot answered. Null if the request failed.
    nlohmann::json data;
    /// Why the request failed, if it did.
    std::optional<std::string> error;
    /// Whether the request failed because the bot didn't answer in time.
    bool timed_out = false;
};

/// Receives the bot's answer to a request.
using ClientResponseCallback = std::function<void(const ClientResponse&)>;

/// Options to create a PipecatClient.
struct PipecatClientOptions {
    /// The transport to connect with. Required.
    std::unique_ptr<Transport> transport;
    /// Receives events. Optional, and must outlive the client.
    PipecatClientCallbacks* callbacks = nullptr;
    /// Whether to disconnect when the bot leaves.
    bool disconnect_on_bot_disconnect = true;
    /// How long connect() waits for the bot to be ready. Zero waits forever.
    std::chrono::milliseconds connect_timeout = std::chrono::seconds(30);
    /// Information about the client, sent to the bot. You can add details
    /// about your app to `platform_details`.
    rtvi::AboutClientData about = rtvi::default_about_client();
};

/// Connects to a Pipecat bot through a transport.
///
/// All methods are thread-safe. start_bot(), connect() and disconnect() return
/// once they're done. Everything else returns right away.
///
/// Sending messages needs a ready bot. Otherwise it throws BotNotReadyError.
/// Messages too large for the transport throw MessageTooLargeError.
class PipecatClient {
   public:
    /// Creates a client. Throws PipecatError if there's no transport.
    explicit PipecatClient(PipecatClientOptions options);

    /// Disconnects, and runs the callbacks still waiting, before returning.
    ~PipecatClient();

    /// Clients can't be copied.
    PipecatClient(const PipecatClient&) = delete;

    /// Clients can't be copied.
    PipecatClient& operator=(const PipecatClient&) = delete;

    /// @name Connection
    /// @{

    /// Prepares the transport, e.g. its audio devices. Optional: start_bot()
    /// and connect() do it when needed.
    void initialize();

    /// Starts a bot.
    ///
    /// Sends `request` to a start endpoint, like Pipecat Cloud or your own
    /// server. The endpoint starts a bot and answers with what the transport
    /// needs to reach it, like a room URL and a token. Pass that answer to
    /// connect().
    ///
    /// You don't need this if your app already knows how to reach the bot.
    /// Throws BotAlreadyStartedError or StartBotError.
    nlohmann::json start_bot(const APIRequest& request);

    /// Connects to the bot and waits until it's ready.
    ///
    /// `transport_params` tell the transport how to reach the bot, like a room
    /// URL and a token. They usually come from start_bot(), but you can also
    /// get them another way, e.g. from your backend. Their format depends on
    /// the transport.
    ///
    /// Throws BotAlreadyStartedError, ConnectionTimeoutError or the
    /// transport's errors, and disconnects if it fails.
    rtvi::BotReadyData connect(
            const nlohmann::json& transport_params = nullptr
    );

    /// Starts a bot and connects to it: start_bot(), then connect() with its
    /// answer.
    rtvi::BotReadyData start_bot_and_connect(const APIRequest& request);

    /// Disconnects from the bot and returns once disconnected, also when
    /// another disconnection is already in progress.
    ///
    /// If start_bot() or connect() are still running, they stop and throw. If
    /// the transport is still connecting, this first waits until it's done.
    void disconnect();

    /// The connection state.
    TransportState state() const;

    /// Whether the client is connected, even if the bot isn't ready yet.
    bool connected() const;

    /// The transport, e.g. to use its own features. Connect and disconnect
    /// with the client, not the transport.
    Transport& transport();

    /// @}

    /// @name Audio
    /// @{

    /// Sends `num_frames` frames of 16-bit PCM user audio to the bot. Returns
    /// the number of frames sent, or 0 if not connected.
    int32_t send_user_audio(const int16_t* frames, size_t num_frames);

    /// Reads up to `num_frames` frames of 16-bit PCM bot audio into `frames`.
    /// Returns the number of frames read, or 0 if not connected.
    int32_t read_bot_audio(int16_t* frames, size_t num_frames);

    /// @}

    /// @name Messages to the bot
    /// @{

    /// Sends text to the bot's LLM, as if the user said it.
    void send_text(
            const std::string& content,
            const rtvi::SendTextOptions& options = {}
    );

    /// Sends an app-defined message to the bot.
    void send_client_message(
            const std::string& type,
            const nlohmann::json& data = nullptr
    );

    /// Asks the bot to leave. The client stays connected.
    void disconnect_bot();

    /// Sends DTMF keys (0-9, * and #), e.g. "123#".
    ///
    /// Throws PipecatError if `buttons` has other characters, and
    /// UnsupportedFeatureError if the bot uses an RTVI version older than 2.1.
    void send_dtmf(const std::string& buttons);

    /// @}

    /// @name Client requests
    /// @{

    /// Sends an app-defined message to the bot, and calls `callback` with its
    /// answer.
    ///
    /// `callback` runs on the client's own thread, like callbacks. The request
    /// fails if the bot doesn't answer within `timeout`. Zero waits forever.
    void send_client_request(
            const std::string& type,
            const nlohmann::json& data,
            ClientResponseCallback callback,
            std::chrono::milliseconds timeout = std::chrono::seconds(10)
    );

    /// Sends an app-defined message to the bot, and returns its answer.
    ///
    /// The future's get() throws MessageError if the request fails, and
    /// RequestTimeoutError if the bot doesn't answer within `timeout`. Zero
    /// waits forever. You can wait for the answer inside a callback.
    std::future<nlohmann::json> send_client_request(
            const std::string& type,
            const nlohmann::json& data = nullptr,
            std::chrono::milliseconds timeout = std::chrono::seconds(10)
    );

    /// @}

   private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace pipecat

#endif
