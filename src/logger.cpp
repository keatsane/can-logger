#include <chrono>
#include <cstdint>
#include <cstdio>
#include <system_error>
#include <thread>

#include "isotp_socket.hpp"
#include "obd.hpp"
#include "scheduler.hpp"

using namespace std::chrono_literals;

namespace {

// what to poll and how often. the 200 ms gap caps the bus at 5 requests/s
constexpr PollEntry kPolls[] = {
    {obd::pid::kCoolant, 2000ms},
    {obd::pid::kRpm, 500ms},
    {obd::pid::kSpeed, 500ms},
};
constexpr Clock::duration kMinGap = 200ms;
constexpr int kReplyTimeoutMs = 100;
constexpr Clock::duration kStatusEvery = 1s;

// running totals and the latest value of each pid, for the status line
struct Stats {
    unsigned replies = 0;
    unsigned timeouts = 0;
    unsigned bad = 0;
    double coolant = 0;
    double rpm = 0;
    double speed = 0;
};

/**
 * @brief Print one status line, e.g. "replies 52  timeouts 0  bad 0  coolant ...".
 * @param s the totals and latest values
 */
void print_status(const Stats& s) {
    std::printf("replies %u  timeouts %u  bad %u  coolant %.1f rpm %.2f speed %.0f\n", s.replies,
                s.timeouts, s.bad, s.coolant, s.rpm, s.speed);
    std::fflush(stdout);
}

}  // namespace

int main() {
    try {
        IsoTpSocket sock("can_obd", obd::kEngineRequestId, obd::kEngineReplyId);
        Scheduler sched(kPolls, kMinGap);
        Stats stats;

        auto next_status = Clock::now() + kStatusEvery;

        uint8_t buf[64];
        while (true) {
            auto now = Clock::now();

            if (now >= next_status) {
                print_status(stats);
                next_status += kStatusEvery;
            }

            if (auto pid = sched.next(now)) {
                sock.send(obd::make_request(*pid));

                auto reply = sock.receive(buf, kReplyTimeoutMs);
                if (!reply) {
                    ++stats.timeouts;
                    continue;
                }

                auto reading = obd::decode(*reply);
                if (!reading) {
                    ++stats.bad;
                    continue;
                }

                ++stats.replies;
                switch (reading->pid) {
                    case obd::pid::kCoolant:
                        stats.coolant = reading->value;
                        break;
                    case obd::pid::kRpm:
                        stats.rpm = reading->value;
                        break;
                    case obd::pid::kSpeed:
                        stats.speed = reading->value;
                        break;
                    default:
                        break;
                }
            } else
                std::this_thread::sleep_for(10ms);
        }
    } catch (const std::system_error& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
