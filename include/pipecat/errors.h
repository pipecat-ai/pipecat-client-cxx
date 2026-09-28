//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_ERRORS_H
#define PIPECAT_ERRORS_H

#include <stdexcept>
#include <string>

namespace pipecat {

// Base class for all errors thrown by this library.
class PipecatError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// start_bot() failed.
class StartBotError : public PipecatError {
   public:
    explicit StartBotError(const std::string& message, int status = 0)
        : PipecatError(message), _status(status) {}

    // HTTP status of the response, or 0 if there was no response.
    int status() const { return _status; }

   private:
    int _status;
};

// The bot didn't become ready within the connect timeout.
class ConnectionTimeoutError : public PipecatError {
   public:
    explicit ConnectionTimeoutError(
            const std::string& message =
                    "Bot did not become ready within the connect timeout"
    )
        : PipecatError(message) {}
};

// The transport failed to connect.
class TransportStartError : public PipecatError {
   public:
    explicit TransportStartError(
            const std::string& message = "Unable to connect the transport"
    )
        : PipecatError(message) {}
};

// The transport can't use the given connection parameters.
class InvalidTransportParamsError : public PipecatError {
   public:
    explicit InvalidTransportParamsError(
            const std::string& message = "Invalid transport connection params"
    )
        : PipecatError(message) {}
};

// Something that needs a ready bot was called before connect() finished.
class BotNotReadyError : public PipecatError {
   public:
    explicit BotNotReadyError(
            const std::string& message =
                    "Bot is not ready, call connect() first"
    )
        : PipecatError(message) {}
};

// start_bot() or connect() was called while already starting or connected.
class BotAlreadyStartedError : public PipecatError {
   public:
    explicit BotAlreadyStartedError(
            const std::string& message =
                    "Client already started, call disconnect() first"
    )
        : PipecatError(message) {}
};

// The transport or the bot doesn't support a feature.
class UnsupportedFeatureError : public PipecatError {
   public:
    explicit UnsupportedFeatureError(
            const std::string& feature,
            const std::string& message = ""
    )
        : PipecatError(
                  feature + " not supported" +
                  (message.empty() ? "" : ": " + message)
          ),
          _feature(feature) {}

    const std::string& feature() const { return _feature; }

   private:
    std::string _feature;
};

// A message is larger than the transport allows.
class MessageTooLargeError : public PipecatError {
   public:
    explicit MessageTooLargeError(
            const std::string& message =
                    "Message size exceeds the transport's limit"
    )
        : PipecatError(message) {}
};

}  // namespace pipecat

#endif
