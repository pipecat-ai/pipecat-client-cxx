//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "rtvi_utils.h"

#include <gtest/gtest.h>

#include <cctype>
#include <thread>

using namespace rtvi;

TEST(GenerateRandomId, IsAlphanumeric) {
    auto id = generate_random_id();
    EXPECT_EQ(id.size(), 10u);
    for (char c: id) {
        EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(c))) << id;
    }
}

TEST(GenerateRandomId, IsUnique) {
    EXPECT_NE(generate_random_id(), generate_random_id());
}

TEST(RTVIQueue, PopsInOrder) {
    RTVIQueue<int> queue;
    queue.push(1);
    queue.push(2);
    EXPECT_EQ(queue.size(), 2u);
    EXPECT_EQ(queue.blocking_pop(), 1);
    EXPECT_EQ(queue.blocking_pop(), 2);
    EXPECT_TRUE(queue.empty());
}

TEST(RTVIQueue, DropsOldestWhenFull) {
    RTVIQueue<int> queue(2);
    queue.push(1);
    queue.push(2);
    queue.push(3);
    EXPECT_EQ(queue.size(), 2u);
    EXPECT_EQ(queue.blocking_pop(), 2);
    EXPECT_EQ(queue.blocking_pop(), 3);
}

TEST(RTVIQueue, StopUnblocksPop) {
    RTVIQueue<int> queue;
    std::optional<int> result = 0;
    std::thread consumer([&] { result = queue.blocking_pop(); });
    queue.stop();
    consumer.join();
    EXPECT_FALSE(result.has_value());
}
