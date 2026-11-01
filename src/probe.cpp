#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
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
    // iso-tp: the kernel handles the length byte, padding and multi-frame messages
    Fd fd(socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP));
    if (fd.value < 0) return fail("socket");

    // obd-ii on can always sends full 8-byte frames, so pad what we send. must come before bind
    can_isotp_options opts{};
    opts.flags = CAN_ISOTP_TX_PADDING;
    opts.txpad_content = 0x00;
    if (setsockopt(fd.value, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0)
        return fail("setsockopt");

    // interface name -> the number the kernel uses for it
    unsigned int ifindex = if_nametoindex("can_obd");
    if (ifindex == 0) return fail("can_obd");

    // send on 7e0, only accept 7e8: the socket filters for us
    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = static_cast<int>(ifindex);
    addr.can_addr.tp.tx_id = obd::kEngineRequestId;
    addr.can_addr.tp.rx_id = obd::kEngineReplyId;
    if (bind(fd.value, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return fail("bind");

    obd::Request req = obd::make_request(obd::pid::kRpm);
    if (write(fd.value, req.data(), req.size()) != static_cast<ssize_t>(req.size()))
        return fail("write");

    pollfd pfd{};
    pfd.fd = fd.value;
    pfd.events = POLLIN;
    int n = poll(&pfd, 1, 100);
    if (n < 0) return fail("poll");
    if (n == 0) {
        std::fprintf(stderr, "no reply within 100 ms\n");
        return 1;
    }

    uint8_t buf[64];  // a whole reply, however many frames it took
    ssize_t len = read(fd.value, buf, sizeof(buf));
    if (len < 0) return fail("read");

    auto reading = obd::decode({buf, static_cast<size_t>(len)});
    if (!reading || reading->pid != obd::pid::kRpm) {
        std::fprintf(stderr, "unexpected reply\n");
        return 1;
    }

    std::printf("rpm: %.2f\n", reading->value);
    return 0;
}
