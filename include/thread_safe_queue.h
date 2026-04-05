#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// Generic thread-safe FIFO queue (header-only template).
// Supports blocking pop (wait_and_pop) and non-blocking try_pop.
template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // Block until an item is available, then move it into 'value'.
    // Returns false only when the queue has been closed and is empty.
    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]{ return !queue_.empty() || closed_; });
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T v = std::move(queue_.front());
        queue_.pop();
        return v;
    }

    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Signal all waiting threads that no more items will be pushed.
    void close() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::queue<T>           queue_;
    bool                    closed_ = false;
};
