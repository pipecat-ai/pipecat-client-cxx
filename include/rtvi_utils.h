//
// Copyright (c) 2024, Daily
//

#ifndef RTVI_UTILS_H
#define RTVI_UTILS_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

namespace rtvi {

std::string generate_random_id();

template<typename T>
class RTVIQueue {
   public:
    RTVIQueue(size_t max_capacity = 0)
        : _max_capacity(max_capacity), _stop(false) {}

    void push(const T& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_max_capacity > 0 && _queue.size() >= _max_capacity) {
            _queue.pop();
        }
        _queue.push(std::move(value));
        _condition.notify_one();
    }

    std::optional<T> blocking_pop() {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this] { return _stop || !_queue.empty(); });

        if (_stop) {
            return std::nullopt;
        }

        T value = _queue.front();
        _queue.pop();
        return std::move(value);
    }

    void stop() {
        std::lock_guard<std::mutex> lock(_mutex);
        _stop = true;
        _condition.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

   private:
    size_t _max_capacity;
    std::atomic<bool> _stop;
    std::queue<T> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _condition;
};

}  // namespace rtvi

#endif
