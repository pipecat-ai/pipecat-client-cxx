//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#ifndef PIPECAT_EVENT_LOOP_H
#define PIPECAT_EVENT_LOOP_H

#include <functional>
#include <memory>
#include <thread>

namespace pipecat {

// Runs tasks one at a time, in the order they were posted, on its own thread.
class EventLoop {
   public:
    EventLoop();

    // Calls stop().
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Queues a task and returns right away. Ignored once the loop has
    // stopped. Tasks must not throw; if they do, the exception is ignored.
    void post(std::function<void()> task);

    // Runs the tasks already posted and stops the thread. When called from a
    // task, the remaining tasks are dropped instead, since the thread can't
    // wait for itself.
    void stop();

    // Whether the caller is running on the loop's thread.
    bool in_loop_thread() const;

   private:
    struct State;

    static void run(std::shared_ptr<State> state);

    // Shared with the thread, so it outlives the loop if the thread is
    // detached.
    std::shared_ptr<State> _state;
    std::thread _thread;
};

}  // namespace pipecat

#endif
