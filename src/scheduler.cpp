#include "scheduler.hpp"

Scheduler::Scheduler(std::span<const PollEntry> entries, Clock::duration min_gap)
    : min_gap_(min_gap) {
    for (const PollEntry& e : entries)
        slots_.push_back(Slot{e, std::nullopt});
}

std::optional<uint8_t> Scheduler::next(Clock::time_point now) {
    if (last_request_ && (now - *last_request_ < min_gap_)) return std::nullopt;

    Slot* best = nullptr;
    Clock::duration best_overdue{};
    for (Slot& s : slots_) {
        if (!s.last_sent) {
            best = &s;
            break;
        }
        Clock::duration overdue = now - (*s.last_sent + s.entry.period);
        if (overdue < Clock::duration::zero()) continue;  // not due yet
        if (!best || overdue > best_overdue) {
            best = &s;
            best_overdue = overdue;
        }
    }

    if (!best) return std::nullopt;

    best->last_sent = now;
    last_request_ = now;
    return best->entry.pid;
}
