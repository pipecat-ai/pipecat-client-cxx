//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "event_loop.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

using pipecat::EventLoop;

TEST(EventLoop, RunsTasksInOrder) {
    std::vector<int> order;
    {
        EventLoop loop;
        for (int i = 0; i < 100; i++) {
            loop.post([&order, i] { order.push_back(i); });
        }
    }
    ASSERT_EQ(order.size(), 100u);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(order[i], i);
    }
}

TEST(EventLoop, RunsOnItsOwnThread) {
    EventLoop loop;
    std::promise<bool> in_loop_thread;
    loop.post([&] { in_loop_thread.set_value(loop.in_loop_thread()); });
    EXPECT_TRUE(in_loop_thread.get_future().get());
    EXPECT_FALSE(loop.in_loop_thread());
}

TEST(EventLoop, StopRunsFollowUpTasks) {
    std::vector<int> order;
    EventLoop loop;
    loop.post([&] {
        order.push_back(1);
        loop.post([&] { order.push_back(2); });
    });
    loop.stop();
    EXPECT_EQ(order, (std::vector<int> {1, 2}));
}

TEST(EventLoop, IgnoresTasksAfterStop) {
    bool ran = false;
    EventLoop loop;
    loop.stop();
    loop.post([&] { ran = true; });
    loop.stop();
    EXPECT_FALSE(ran);
}

TEST(EventLoop, KeepsRunningAfterException) {
    bool ran = false;
    {
        EventLoop loop;
        loop.post([] { throw std::runtime_error("oops"); });
        loop.post([&] { ran = true; });
    }
    EXPECT_TRUE(ran);
}

TEST(EventLoop, CanBeDestroyedFromATask) {
    auto* loop = new EventLoop();
    std::promise<void> posted;
    std::promise<void> done;
    bool ran_after = false;
    loop->post([&, all_posted = posted.get_future().share()] {
        // Wait until the test is done posting, so it doesn't touch the loop
        // after it's deleted.
        all_posted.wait();
        delete loop;
        done.set_value();
    });
    loop->post([&] { ran_after = true; });
    posted.set_value();
    ASSERT_EQ(
            done.get_future().wait_for(std::chrono::seconds(5)),
            std::future_status::ready
    );
    // Tasks still queued are dropped.
    EXPECT_FALSE(ran_after);
}
