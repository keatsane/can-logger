#include "isotp_socket.hpp"

#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

namespace {

// throw errno as an exception, naming the call that failed. the socket stays open
[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

}  // namespace

IsoTpSocket::IsoTpSocket(const char* ifname, canid_t tx_id, canid_t rx_id)
    : fd_(socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)) {
    if (fd_ < 0) fail_open("socket");

    // obd-ii on can always sends full 8-byte frames, so pad what we send. must come before bind
    can_isotp_options opts{};
    opts.flags = CAN_ISOTP_TX_PADDING;
    opts.txpad_content = 0x00;
    if (setsockopt(fd_, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0)
        fail_open("setsockopt");

    // interface name -> the number the kernel uses for it
    unsigned int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) fail_open(ifname);

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = static_cast<int>(ifindex);
    addr.can_addr.tp.tx_id = tx_id;
    addr.can_addr.tp.rx_id = rx_id;
    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) fail_open("bind");
}

// a constructor that throws never finishes, so the destructor won't run: close the fd here
void IsoTpSocket::fail_open(const char* what) {
    int err = errno;  // close() below could overwrite errno
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    errno = err;
    throw_errno(what);
}

IsoTpSocket::~IsoTpSocket() {
    if (fd_ >= 0) close(fd_);
}

IsoTpSocket::IsoTpSocket(IsoTpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

IsoTpSocket& IsoTpSocket::operator=(IsoTpSocket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void IsoTpSocket::send(std::span<const uint8_t> payload) {
    ssize_t size = write(fd_, payload.data(), payload.size());
    if (size != static_cast<ssize_t>(payload.size())) {
        if (size >= 0) errno = EIO;
        throw_errno("write");
    }
}

std::optional<std::span<const uint8_t>> IsoTpSocket::receive(std::span<uint8_t> buf,
                                                             int timeout_ms) {
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;
    int n = poll(&pfd, 1, timeout_ms);
    if (n < 0) throw_errno("poll");
    if (n == 0) return std::nullopt;

    ssize_t size = read(fd_, buf.data(), buf.size());
    if (size < 0) throw_errno("read");
    return buf.first(static_cast<size_t>(size));
}
