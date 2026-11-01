#include "obd.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>

namespace {

// a reply as the ecu sends it: id 7e8, 8 bytes, unused bytes padded with 0x55
can_frame reply(std::initializer_list<uint8_t> bytes) {
    can_frame f{};
    f.can_id = obd::kEngineReplyId;
    f.len = 8;
    std::fill(std::begin(f.data), std::end(f.data), 0x55);
    std::copy(bytes.begin(), bytes.end(), f.data);
    return f;
}

}  // namespace

TEST(Decode, Rpm) {
    auto r = obd::decode(reply({0x04, 0x41, 0x0C, 0x1B, 0x05}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, 0x0C);
    EXPECT_DOUBLE_EQ(r->value, 1729.25);
}

TEST(Decode, RpmMax) {
    auto r = obd::decode(reply({0x04, 0x41, 0x0C, 0xFF, 0xFF}));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->value, 16383.75);
}

TEST(Decode, WrongService) {
    auto r = obd::decode(reply({0x04, 0x42, 0x0C, 0x1B, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, TooShort) {
    auto r = obd::decode(reply({0x03, 0x41, 0x0C, 0x1B, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, UnknownPid) {
    auto r = obd::decode(reply({0x04, 0x41, 0x2F, 0x1B, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, ShortFrame) {
    auto f = reply({0x04, 0x41, 0x0C, 0x1B, 0x05});
    f.len = 4;
    auto r = obd::decode(f);
    EXPECT_FALSE(r.has_value());
}
