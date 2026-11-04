#include "scheduler.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>

using namespace std::chrono_literals;

namespace {

// one pid twice a second, the other every 2 s, at most one request per 200 ms
constexpr std::array<PollEntry, 2> kTable{{{0x05, 500ms}, {0x0C, 2000ms}}};
constexpr Clock::duration kGap = 200ms;

// a fake clock: t(0) is the start, t(250) is 250 ms later
Clock::time_point t(int ms) {
    return Clock::time_point{} + std::chrono::milliseconds(ms);
}

}  // namespace

TEST(Scheduler, FirstCallAsksForFirstPid) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
}

TEST(Scheduler, GapBlocksSecondRequest) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
    EXPECT_EQ(s.next(t(0)), std::nullopt);
    EXPECT_EQ(s.next(t(199)), std::nullopt);
}

TEST(Scheduler, NeverSentGoesNext) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
    EXPECT_EQ(s.next(t(200)), 0x0C);
}

TEST(Scheduler, NotDueBeforePeriod) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
    EXPECT_EQ(s.next(t(200)), 0x0C);
    EXPECT_EQ(s.next(t(400)), std::nullopt);
}

TEST(Scheduler, DueAgainAfterPeriod) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
    EXPECT_EQ(s.next(t(200)), 0x0C);
    EXPECT_EQ(s.next(t(400)), std::nullopt);
    EXPECT_EQ(s.next(t(500)), 0x05);
}

TEST(Scheduler, SlowPidNotStarved) {
    Scheduler s(kTable, kGap);
    EXPECT_EQ(s.next(t(0)), 0x05);
    EXPECT_EQ(s.next(t(200)), 0x0C);
    EXPECT_EQ(s.next(t(500)), 0x05);
    EXPECT_EQ(s.next(t(1000)), 0x05);
    EXPECT_EQ(s.next(t(1500)), 0x05);
    EXPECT_EQ(s.next(t(2000)), 0x05);
    EXPECT_EQ(s.next(t(2200)), 0x0C);
}

TEST(Scheduler, EmptyTable) {
    Scheduler s({}, kGap);
    EXPECT_EQ(s.next(t(0)), std::nullopt);
}
