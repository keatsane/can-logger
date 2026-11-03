#include "isotp_socket.hpp"

#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

IsoTpSocket::IsoTpSocket(const char* ifname, canid_t tx_id, canid_t rx_id)
    : fd_(socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)) {
    if (fd_.get() < 0) throw_errno("socket");

    // obd-ii on can always sends full 8-byte frames, so pad what we send. must come before bind
    can_isotp_options opts{};
    opts.flags = CAN_ISOTP_TX_PADDING;
    opts.txpad_content = 0x00;
    if (setsockopt(fd_.get(), SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0)
        throw_errno("setsockopt");

    // interface name -> the number the kernel uses for it
    unsigned int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) throw_errno(ifname);

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = static_cast<int>(ifindex);
    addr.can_addr.tp.tx_id = tx_id;
    addr.can_addr.tp.rx_id = rx_id;
    if (bind(fd_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) throw_errno("bind");
}

void IsoTpSocket::send(std::span<const uint8_t> payload) {
    write_all(fd_.get(), payload);
}

std::optional<std::span<const uint8_t>> IsoTpSocket::receive(std::span<uint8_t> buf,
                                                             int timeout_ms) {
    pollfd pfd{};
    pfd.fd = fd_.get();
    pfd.events = POLLIN;
    int n = poll(&pfd, 1, timeout_ms);
    if (n < 0) throw_errno("poll");
    if (n == 0) return std::nullopt;

    ssize_t size = read(fd_.get(), buf.data(), buf.size());
    if (size < 0) throw_errno("read");
    return buf.first(static_cast<size_t>(size));
}
