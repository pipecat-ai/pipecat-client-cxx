//
// Copyright (c) 2024, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "rtvi_messages.h"

#include <gtest/gtest.h>

using namespace rtvi;

TEST(RTVIMessage, HasEnvelope) {
    auto message = RTVIMessage::message("client-ready");
    EXPECT_EQ(message["label"], "rtvi-ai");
    EXPECT_EQ(message["type"], "client-ready");
    EXPECT_FALSE(message["id"].get<std::string>().empty());
    EXPECT_FALSE(message.contains("data"));
}

TEST(RTVIMessage, HasData) {
    auto message = RTVIMessage::message("send-text", {{"content", "hello"}});
    EXPECT_EQ(message["data"]["content"], "hello");
}
