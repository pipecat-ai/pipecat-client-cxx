//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/pipecat.h>
#include <pipecat/smallwebrtc/transport.h>

#include <memory>

// Tries to send an offer to a server that isn't there through the installed
// package, so missing libraries show up as link errors.
int main() {
    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::SmallWebRTCTransport>();

    pipecat::PipecatClient client(std::move(options));
    try {
        client.connect(
                {{"webrtcRequestParams",
                  {{"endpoint", "http://127.0.0.1:1/api/offer"}}}}
        );
    } catch (const pipecat::TransportStartError&) {
        return 0;
    }
    return 1;
}
