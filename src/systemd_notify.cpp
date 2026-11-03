#include "systemd_notify.hpp"

#include <sys/socket.h>
#include <sys/un.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "unique_fd.hpp"

bool notify_systemd(const char* state) {
    const char* path = std::getenv("NOTIFY_SOCKET");
    if (!path) return false;

    sockaddr_un addr{};
    std::size_t path_len = std::strlen(path);
    if (path_len == 0 || path_len >= sizeof(addr.sun_path)) return false;

    UniqueFd fd(socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0));
    if (fd.get() < 0) return false;

    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path, path_len);
    if (path[0] == '@') addr.sun_path[0] = '\0';  // abstract socket
    auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path_len);

    std::size_t state_len = std::strlen(state);
    ssize_t n = sendto(fd.get(), state, state_len, 0, reinterpret_cast<sockaddr*>(&addr), addr_len);
    return n == static_cast<ssize_t>(state_len);
}
