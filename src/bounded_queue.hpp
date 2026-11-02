/*
 * a fixed-size queue that two threads can share: one pushes, the other pops.
 * pushing never waits - when the queue is full the item is dropped and counted, so a slow
 * consumer can't stall the producer. no allocation after construction (std::array storage).
 * a template, so the whole thing lives in this header.
 */
#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

template <typename T, std::size_t N>
class BoundedQueue {
    static_assert(N > 0, "a queue needs room for at least one item");

public:
    /**
     * @brief Add an item without waiting.
     * @param item the item to copy in
     * @return true if it was added, false if the queue was full (the item is dropped)
     */
    bool try_push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(m_);

            if (count_ == N) {
                ++dropped_;
                return false;
            }
            items_[(head_ + count_) % N] = item;
            ++count_;
        }

        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Take the oldest item, waiting up to timeout for one to arrive.
     * @param timeout how long to wait if the queue is empty
     * @return the item, or std::nullopt if nothing arrived in time
     */
    std::optional<T> pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_);
        if (!not_empty_.wait_for(lock, timeout, [this] { return count_ > 0; })) return std::nullopt;
        T popped = items_[head_];
        head_ = (head_ + 1) % N;
        --count_;
        return popped;
    }

    /// how many pushes were dropped because the queue was full
    std::size_t dropped() const {
        std::lock_guard<std::mutex> lock(m_);

        return dropped_;
    }

private:
    mutable std::mutex m_;  // guards everything below. mutable so dropped() can lock it
    std::condition_variable not_empty_;
    std::array<T, N> items_{};
    std::size_t head_ = 0;  // index of the oldest item
    std::size_t count_ = 0;
    std::size_t dropped_ = 0;
};
