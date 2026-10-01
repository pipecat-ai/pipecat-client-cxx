//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/pipecat.h>

#include <memory>

// Links the whole client, so a missing dependency (libcurl, threads) in the
// installed package shows up as a link error.
class NullTransport : public pipecat::Transport {
   public:
    void initialize(pipecat::TransportObserver*) override {}
    void connect(const nlohmann::json&) override {}
    void disconnect() override {}
    void send_ready_message(const pipecat::rtvi::Message&) override {}
    void send_message(const pipecat::rtvi::Message&) override {}
    int32_t send_user_audio(const int16_t*, size_t) override { return 0; }
    int32_t read_bot_audio(int16_t*, size_t) override { return 0; }
};

int main() {
    pipecat::PipecatClientOptions options;
    options.transport = std::make_unique<NullTransport>();
    bool about_ok = options.about.library_version == PIPECAT_VERSION;

    pipecat::PipecatClient client(std::move(options));
    client.initialize();

    bool ok =
            about_ok && client.state() == pipecat::TransportState::Initialized;
    return ok ? 0 : 1;
}
