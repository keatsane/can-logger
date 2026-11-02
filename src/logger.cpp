#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <system_error>
#include <thread>

#include "bounded_queue.hpp"
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

// what the writer has seen so far
struct Stats {
    unsigned replies = 0;
    std::optional<double> coolant;
    std::optional<double> rpm;
    std::optional<double> speed;
};

// "  rpm  1396.50", or "  rpm        -" before the first reading
void print_field(const char* name, std::optional<double> v, int decimals) {
    if (v) {
        std::printf("  %s %8.*f", name, decimals, *v);
    } else {
        std::printf("  %s %8s", name, "-");
    }
}

/**
 * @brief Print one status line: counters, then the latest value of each pid.
 * @param s replies and latest values, from the writer
 * @param sent requests sent, from the poller
 * @param bad replies that didn't decode, from the receiver
 * @param dropped readings lost because the queue was full
 */
void print_status(const Stats& s, unsigned sent, unsigned bad, std::size_t dropped) {
    std::printf("sent %5u  replies %5u  bad %3u  dropped %3zu", sent, s.replies, bad, dropped);
    print_field("coolant", s.coolant, 1);
    print_field("rpm", s.rpm, 2);
    print_field("speed", s.speed, 0);
    std::printf("\n");
    std::fflush(stdout);
}

}  // namespace

int main() {
    try {
        // shared by the threads below, so created before any of them start
        IsoTpSocket sock("can_obd", obd::kEngineRequestId, obd::kEngineReplyId);
        BoundedQueue<obd::Reading, 64> readings;
        std::atomic<bool> running{true};
        std::atomic<unsigned> sent{0};
        std::atomic<unsigned> bad{0};

        // poller: sends requests on schedule, never waits for replies
        std::thread poller([&] {
            try {
                Scheduler sched(kPolls, kMinGap);
                while (running) {
                    if (auto pid = sched.next(Clock::now())) {
                        sock.send(obd::make_request(*pid));
                        ++sent;
                    } else {
                        std::this_thread::sleep_for(10ms);
                    }
                }
            } catch (const std::system_error& e) {
                std::fprintf(stderr, "poller: %s\n", e.what());
                running = false;
            }
        });

        // receiver: decodes each reply and hands it to the writer through the queue
        std::thread receiver([&] {
            try {
                uint8_t buf[64];
                while (running) {
                    auto reply = sock.receive(buf, kReplyTimeoutMs);
                    if (!reply) continue;

                    auto reading = obd::decode(*reply);
                    if (!reading) {
                        ++bad;
                        continue;
                    }
                    readings.try_push(*reading);
                }
            } catch (const std::system_error& e) {
                std::fprintf(stderr, "receiver: %s\n", e.what());
                running = false;
            }
        });

        // writer, on this thread: takes readings off the queue, prints the status line
        Stats stats;
        auto next_status = Clock::now() + kStatusEvery;
        while (running) {
            if (auto r = readings.pop(100ms)) {
                ++stats.replies;
                switch (r->pid) {
                    case obd::pid::kCoolant:
                        stats.coolant = r->value;
                        break;
                    case obd::pid::kRpm:
                        stats.rpm = r->value;
                        break;
                    case obd::pid::kSpeed:
                        stats.speed = r->value;
                        break;
                    default:
                        break;
                }
            }

            if (Clock::now() >= next_status) {
                print_status(stats, sent, bad, readings.dropped());
                next_status += kStatusEvery;
            }
        }

        // threads use sock and readings, so they must finish before those are destroyed
        poller.join();
        receiver.join();
    } catch (const std::system_error& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
