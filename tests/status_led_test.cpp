#include "status_led.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace {

std::string read_all(const std::string& path) {
    std::ifstream f(path);
    std::string got;
    f >> got;
    return got;
}

}  // namespace

// a plain file behaves like the sysfs one, so no real led is needed
TEST(StatusLed, WritesTheValue) {
    std::string path = testing::TempDir() + "status_led_test";
    std::ofstream(path).close();

    StatusLed led(path.c_str());
    EXPECT_TRUE(led.set(true));
    EXPECT_EQ(read_all(path), "1");
    EXPECT_TRUE(led.set(false));
    EXPECT_EQ(read_all(path), "0");
}

TEST(StatusLed, MissingFileDoesNothing) {
    StatusLed led("/nonexistent/led");
    EXPECT_FALSE(led.set(true));
}
