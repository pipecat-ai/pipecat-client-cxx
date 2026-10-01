//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_TESTS_HELPERS_H
#define PIPECAT_TESTS_HELPERS_H

#include "fake_transport.h"
#include "recorder.h"

#include "pipecat/client.h"

#include <functional>
#include <memory>

struct TestClient {
    FakeTransport* transport;
    std::unique_ptr<pipecat::PipecatClient> client;
};

// A client with a FakeTransport, reporting to `recorder`.
inline TestClient make_client(
        Recorder& recorder,
        std::function<void(pipecat::PipecatClientOptions&)> configure = {}
) {
    auto transport = std::make_unique<FakeTransport>();
    FakeTransport* fake = transport.get();

    pipecat::PipecatClientOptions options;
    options.transport = std::move(transport);
    options.callbacks = &recorder;
    options.connect_timeout = std::chrono::seconds(5);
    if (configure) {
        configure(options);
    }
    return {fake, std::make_unique<pipecat::PipecatClient>(std::move(options))};
}

#endif
