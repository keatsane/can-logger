#include "bounded_queue.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

TEST(BoundedQueue, PushThenPop) {
    BoundedQueue<int, 4> q;
    EXPECT_TRUE(q.try_push(7));
    EXPECT_EQ(q.pop(10ms), 7);
}

TEST(BoundedQueue, PopEmptyTimesOut) {
    BoundedQueue<int, 4> q;
    EXPECT_EQ(q.pop(10ms), std::nullopt);
}

TEST(BoundedQueue, FifoOrder) {
    BoundedQueue<int, 4> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_EQ(q.pop(10ms), 1);
    EXPECT_EQ(q.pop(10ms), 2);
    EXPECT_EQ(q.pop(10ms), 3);
}

TEST(BoundedQueue, FullDropsAndCounts) {
    BoundedQueue<int, 2> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));
    EXPECT_EQ(q.dropped(), 1u);
}

TEST(BoundedQueue, WrapsAround) {
    BoundedQueue<int, 2> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_EQ(q.pop(10ms), 1);
    EXPECT_TRUE(q.try_push(3));
    EXPECT_EQ(q.pop(10ms), 2);
    EXPECT_EQ(q.pop(10ms), 3);
}

TEST(BoundedQueue, PopWaitsForPush) {
    BoundedQueue<int, 4> q;
    std::thread producer([&q] {
        std::this_thread::sleep_for(20ms);
        q.try_push(5);
    });
    EXPECT_EQ(q.pop(1s), 5);
    producer.join();
}

TEST(BoundedQueue, ProducerConsumer) {
    BoundedQueue<int, 8> q;
    std::thread producer([&q] {
        for (int i = 1; i <= 1000; i++)
            q.try_push(i);
    });

    std::size_t received = 0;
    int last = 0;
    while (received + q.dropped() < 1000) {  // until every push is accounted for
        auto item = q.pop(1s);
        if (!item) break;        // stuck: the check below will fail and say so
        EXPECT_GT(*item, last);  // order survived
        last = *item;
        ++received;
    }
    producer.join();
    EXPECT_EQ(received + q.dropped(), 1000u);
}
