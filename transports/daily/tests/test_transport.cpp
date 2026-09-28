//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include <pipecat/daily/transport.h>
#include <pipecat/pipecat.h>

#include <gtest/gtest.h>

#include <memory>

using namespace pipecat;

namespace {

std::unique_ptr<PipecatClient> make_client() {
    PipecatClientOptions options;
    options.transport = std::make_unique<DailyTransport>();
    return std::make_unique<PipecatClient>(std::move(options));
}

}  // namespace

// daily-core is shared by all transports, so several clients can use Daily at
// the same time, and it's set up again after the last one is gone.
TEST(DailyTransport, SeveralTransportsAtOnce) {
    auto first = make_client();
    auto second = make_client();
    first->initialize();
    second->initialize();
    EXPECT_EQ(first->state(), TransportState::Initialized);
    EXPECT_EQ(second->state(), TransportState::Initialized);

    first.reset();
    auto third = make_client();
    third->initialize();
    EXPECT_EQ(third->state(), TransportState::Initialized);

    second.reset();
    third.reset();

    auto fourth = make_client();
    fourth->initialize();
    EXPECT_EQ(fourth->state(), TransportState::Initialized);
}
