/*
 * decides which pid to ask for next. each pid has its own period, and there's a minimum gap
 * between any two requests so the bus never gets flooded. time is passed in, never read here,
 * so the tests can drive it with fake times.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

using Clock = std::chrono::steady_clock;

/// one pid and how often to ask for it
struct PollEntry {
    uint8_t pid;
    Clock::duration period;
};

class Scheduler {
public:
    /**
     * @brief Set up the poll table.
     * @param entries pids in priority order: on a tie, the earlier one goes first
     * @param min_gap shortest allowed time between any two requests
     */
    Scheduler(std::span<const PollEntry> entries, Clock::duration min_gap);

    /**
     * @brief Pick the pid to request at time now, and record that it was sent.
     * @param now the current time
     * @return the most overdue pid, or std::nullopt if none is due or min_gap hasn't passed
     */
    std::optional<uint8_t> next(Clock::time_point now);

private:
    struct Slot {
        PollEntry entry;
        std::optional<Clock::time_point> last_sent;  // nullopt = never sent, so always due
    };

    std::vector<Slot> slots_;
    Clock::duration min_gap_;
    std::optional<Clock::time_point> last_request_;  // nullopt = nothing sent yet
};
