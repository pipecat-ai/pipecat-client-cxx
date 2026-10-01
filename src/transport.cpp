//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "pipecat/transport.h"

namespace pipecat {

const char* to_string(TransportState state) {
    switch (state) {
    case TransportState::Disconnected:
        return "disconnected";
    case TransportState::Initializing:
        return "initializing";
    case TransportState::Initialized:
        return "initialized";
    case TransportState::Authenticating:
        return "authenticating";
    case TransportState::Authenticated:
        return "authenticated";
    case TransportState::Connecting:
        return "connecting";
    case TransportState::Connected:
        return "connected";
    case TransportState::Ready:
        return "ready";
    case TransportState::Disconnecting:
        return "disconnecting";
    case TransportState::Error:
        return "error";
    }
    return "";
}

}  // namespace pipecat
