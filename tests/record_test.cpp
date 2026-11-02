#include "record.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

namespace {

std::vector<uint8_t> bytes_of(const char* s) {
    std::vector<uint8_t> v;
    for (; *s; ++s)
        v.push_back(static_cast<uint8_t>(*s));
    return v;
}

}  // namespace

// the standard check value every crc-32 implementation is tested against
TEST(Crc32, CheckValue) {
    EXPECT_EQ(record::crc32(bytes_of("123456789")), 0xCBF43926u);
}

TEST(Crc32, Empty) {
    EXPECT_EQ(record::crc32({}), 0u);
}

TEST(Crc32, OneByteChanges) {
    EXPECT_NE(record::crc32(bytes_of("123456789")), record::crc32(bytes_of("123456780")));
}

// every field uses different bytes, so a field in the wrong place or order shows up
TEST(Encode, Layout) {
    auto b = record::encode(0x01020304, 0x1122334455667788, obd::Reading{0x0C, 1729.25});
    const uint8_t expected[24] = {
        0x43, 0x4C,                                      // magic
        0x01,                                            // version
        0x0C,                                            // pid
        0x04, 0x03, 0x02, 0x01,                          // seq
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,  // time
        0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x9B, 0x40,  // 1729.25 as a double
    };
    for (int i = 0; i < 24; ++i)
        EXPECT_EQ(b[i], expected[i]) << "byte " << i;
}

TEST(Encode, CrcCoversHeader) {
    auto b = record::encode(7, 1000, obd::Reading{0x05, 90.0});
    uint32_t stored = b[24] | (b[25] << 8) | (b[26] << 16) | (static_cast<uint32_t>(b[27]) << 24);
    EXPECT_EQ(stored, record::crc32(std::span<const uint8_t>(b.data(), 24)));
    // python's zlib.crc32 of the same 24 bytes, so the decoder will agree
    EXPECT_EQ(stored, 0xDFB912C9u);
}
