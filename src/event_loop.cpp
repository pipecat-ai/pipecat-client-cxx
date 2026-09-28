//
// Copyright (c) 2024-2026, Daily
//
// SPDX-License-Identifier: BSD-2-Clause
//

#include "event_loop.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace pipecat {

struct EventLoop::State {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::function<void()>> tasks;
    // Stop once the queue is empty.
    bool stopping = false;
    // Stop right away, dropping queued tasks.
    bool stopped = false;
};

EventLoop::EventLoop()
    : _state(std::make_shared<State>()), _thread(&EventLoop::run, _state) {}

EventLoop::~EventLoop() {
    stop();
}

void EventLoop::post(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        // Tasks can still post while the loop drains, so their follow-ups run
        // too.
        if (_state->stopped) {
            return;
        }
        _state->tasks.push_back(std::move(task));
    }
    _state->cv.notify_one();
}

void EventLoop::stop() {
    if (!_thread.joinable()) {
        return;
    }

    if (in_loop_thread()) {
        {
            std::lock_guard<std::mutex> lock(_state->mutex);
            _state->stopped = true;
            _state->tasks.clear();
        }
        _thread.detach();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        _state->stopping = true;
    }
    _state->cv.notify_one();
    _thread.join();

    std::lock_guard<std::mutex> lock(_state->mutex);
    _state->stopped = true;
}

bool EventLoop::in_loop_thread() const {
    return std::this_thread::get_id() == _thread.get_id();
}

void EventLoop::run(std::shared_ptr<State> state) {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->cv.wait(lock, [&] {
                return state->stopping || state->stopped ||
                       !state->tasks.empty();
            });
            if (state->stopped || state->tasks.empty()) {
                return;
            }
            task = std::move(state->tasks.front());
            state->tasks.pop_front();
        }

        try {
            task();
        } catch (...) {
            // Tasks must not throw. Keep the loop running if they do.
        }
    }
}

}  // namespace pipecat
