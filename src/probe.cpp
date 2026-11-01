#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "obd.hpp"

// owns a file descriptor and closes it when it goes out of scope
struct Fd {
    int value;
    explicit Fd(int v) : value(v) {}
    ~Fd() {
        if (value >= 0) close(value);
    }
    Fd(const Fd&) = delete;  // no copies: two owners would close it twice
    Fd& operator=(const Fd&) = delete;
};

/**
 * @brief Print which call failed and why.
 * @param what name of the failed call
 * @return exit code for main
 */
int fail(const char* what) {
    std::fprintf(stderr, "%s: %s\n", what, std::strerror(errno));
    return 1;
}

int main() {
    Fd fd(socket(PF_CAN, SOCK_RAW, CAN_RAW));
    if (fd.value < 0) return fail("socket");

    // interface name -> the number the kernel uses for it
    unsigned int ifindex = if_nametoindex("can_obd");
    if (ifindex == 0) return fail("can_obd");

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = static_cast<int>(ifindex);
    if (bind(fd.value, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return fail("bind");

    // only engine ecu replies: all 11 id bits, standard-id data frames only
    can_filter filter{};
    filter.can_id = obd::kEngineReplyId;
    filter.can_mask = CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK;
    if (setsockopt(fd.value, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter)) < 0)
        return fail("setsockopt");

    can_frame req = obd::make_request(obd::pid::kRpm);
    if (write(fd.value, &req, sizeof(req)) != sizeof(req)) return fail("write");

    pollfd pfd{};
    pfd.fd = fd.value;
    pfd.events = POLLIN;
    int n = poll(&pfd, 1, 100);
    if (n < 0) return fail("poll");
    if (n == 0) {
        std::fprintf(stderr, "no reply within 100 ms\n");
        return 1;
    }

    can_frame resp{};
    if (read(fd.value, &resp, sizeof(resp)) != sizeof(resp)) return fail("read");

    auto reading = obd::decode(resp);
    if (!reading || reading->pid != obd::pid::kRpm) {
        std::fprintf(stderr, "unexpected reply\n");
        return 1;
    }

    std::printf("rpm: %.2f\n", reading->value);
    return 0;
}
