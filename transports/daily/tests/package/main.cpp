//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/daily/transport.h>
#include <pipecat/pipecat.h>

#include <memory>

// Initializes Daily through the installed package, so missing libraries show
// up as link errors.
int main() {
    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<pipecat::DailyTransport>();

    pipecat::PipecatClient client(std::move(options));
    client.initialize();

    return client.state() == pipecat::TransportState::Initialized ? 0 : 1;
}
