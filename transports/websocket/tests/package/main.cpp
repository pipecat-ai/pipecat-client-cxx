//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/pipecat.h>
#include <pipecat/websocket/transport.h>

#include <memory>

// Tries to connect to a WebSocket that isn't there through the installed
// package, so missing libraries show up as link errors.
int main() {
    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::WebSocketTransport>();

    pipecat::PipecatClient client(std::move(options));
    try {
        client.connect({{"wsUrl", "ws://127.0.0.1:1"}});
    } catch (const pipecat::TransportStartError&) {
        return 0;
    }
    return 1;
}
