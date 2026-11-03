#include "log_file.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <system_error>

TEST(LogFile, WriteThenReadBack) {
    std::string path = testing::TempDir() + "log_file_test.bin";
    std::remove(path.c_str());  // left over from an earlier run

    const uint8_t data[] = {0x43, 0x4C, 0x01};
    {
        LogFile file(path.c_str());
        file.write(data);
        file.sync();
    }  // the LogFile closes here

    std::ifstream f(path, std::ios::binary);
    char got[4] = {};
    f.read(got, sizeof(got));  // ask for one more byte than we wrote

    // the file holds exactly what we wrote, nothing more
    EXPECT_EQ(f.gcount(), 3);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(static_cast<uint8_t>(got[i]), data[i]) << "byte " << i;
}

// O_EXCL: a second logger started in the same second must not overwrite the first log
TEST(LogFile, RefusesToOverwrite) {
    std::string path = testing::TempDir() + "log_file_exists.bin";
    std::remove(path.c_str());

    LogFile first(path.c_str());
    EXPECT_THROW(LogFile second(path.c_str()), std::system_error);
}
