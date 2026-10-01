//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

/// @file
/// Errors thrown by the client.

#ifndef PIPECAT_ERRORS_H
#define PIPECAT_ERRORS_H

#include <stdexcept>
#include <string>

namespace pipecat {

/// Base class of all the errors thrown by the client.
class PipecatError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/// Starting the bot failed. Thrown by PipecatClient::start_bot().
class StartBotError : public PipecatError {
   public:
    /// Creates the error. `status` is the HTTP status of the answer, or 0 if
    /// there was no answer.
    explicit StartBotError(const std::string& message, int status = 0)
        : PipecatError(message), _status(status) {}

    /// The HTTP status of the answer, or 0 if there was no answer.
    int status() const { return _status; }

   private:
    int _status;
};

/// The bot didn't become ready in time. Thrown by PipecatClient::connect().
class ConnectionTimeoutError : public PipecatError {
   public:
    /// Creates the error.
    explicit ConnectionTimeoutError(
            const std::string& message =
                    "Bot did not become ready within the connect timeout"
    )
        : PipecatError(message) {}
};

/// The transport couldn't connect. Thrown by Transport::connect().
class TransportStartError : public PipecatError {
   public:
    /// Creates the error.
    explicit TransportStartError(
            const std::string& message = "Unable to connect the transport"
    )
        : PipecatError(message) {}
};

/// The transport can't use the connection parameters it was given. Thrown by
/// Transport::connect().
class InvalidTransportParamsError : public PipecatError {
   public:
    /// Creates the error.
    explicit InvalidTransportParamsError(
            const std::string& message = "Invalid transport connection params"
    )
        : PipecatError(message) {}
};

/// The bot isn't ready. Thrown when sending messages before
/// PipecatClient::connect() has finished.
class BotNotReadyError : public PipecatError {
   public:
    /// Creates the error.
    explicit BotNotReadyError(
            const std::string& message =
                    "Bot is not ready, call connect() first"
    )
        : PipecatError(message) {}
};

/// The client is already starting or connected. Thrown by
/// PipecatClient::start_bot() and PipecatClient::connect().
class BotAlreadyStartedError : public PipecatError {
   public:
    /// Creates the error.
    explicit BotAlreadyStartedError(
            const std::string& message =
                    "Client already started, call disconnect() first"
    )
        : PipecatError(message) {}
};

/// The transport or the bot doesn't support a feature.
class UnsupportedFeatureError : public PipecatError {
   public:
    /// Creates the error for `feature`, with an optional explanation in
    /// `message`.
    explicit UnsupportedFeatureError(
            const std::string& feature,
            const std::string& message = ""
    )
        : PipecatError(
                  feature + " not supported" +
                  (message.empty() ? "" : ": " + message)
          ),
          _feature(feature) {}

    /// The feature that isn't supported, e.g. "DTMF".
    const std::string& feature() const { return _feature; }

   private:
    std::string _feature;
};

/// A request to the bot failed, because the bot answered with an error or the
/// client disconnected first. See PipecatClient::send_client_request().
class MessageError : public PipecatError {
   public:
    using PipecatError::PipecatError;
};

/// The bot didn't answer a request in time. See
/// PipecatClient::send_client_request().
class RequestTimeoutError : public MessageError {
   public:
    /// Creates the error.
    explicit RequestTimeoutError(
            const std::string& message = "Timed out waiting for a response"
    )
        : MessageError(message) {}
};

/// A message is too large for the transport.
class MessageTooLargeError : public PipecatError {
   public:
    /// Creates the error.
    explicit MessageTooLargeError(
            const std::string& message =
                    "Message size exceeds the transport's limit"
    )
        : PipecatError(message) {}
};

}  // namespace pipecat

#endif
