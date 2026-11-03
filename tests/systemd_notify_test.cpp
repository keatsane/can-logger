#include "systemd_notify.hpp"

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "unique_fd.hpp"

TEST(NotifySystemd, NoSocketDoesNothing) {
    unsetenv("NOTIFY_SOCKET");
    EXPECT_FALSE(notify_systemd("WATCHDOG=1"));
}

TEST(NotifySystemd, SendsTheMessage) {
    std::string path = testing::TempDir() + "notify_test.sock";
    std::remove(path.c_str());  // left over from an earlier run

    // stands in for systemd: a socket bound to a path, waiting for messages
    UniqueFd fake_systemd(socket(AF_UNIX, SOCK_DGRAM, 0));
    ASSERT_GE(fake_systemd.get(), 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(bind(fake_systemd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    setenv("NOTIFY_SOCKET", path.c_str(), 1);
    EXPECT_TRUE(notify_systemd("WATCHDOG=1"));
    unsetenv("NOTIFY_SOCKET");

    // MSG_DONTWAIT: if nothing arrived, fail now instead of hanging forever
    char buf[32] = {};
    ssize_t n = recv(fake_systemd.get(), buf, sizeof(buf) - 1, MSG_DONTWAIT);
    EXPECT_EQ(n, 10);
    EXPECT_STREQ(buf, "WATCHDOG=1");

    std::remove(path.c_str());
}
