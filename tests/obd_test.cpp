#include "obd.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

constexpr uint8_t kUnsupportedPid = 0xFF;  // not one we decode

// a reply payload as an iso-tp read returns it: no length byte, no padding
std::vector<uint8_t> reply(std::initializer_list<uint8_t> bytes) {
    return bytes;
}

}  // namespace

TEST(Decode, Empty) {
    auto r = obd::decode(reply({}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, NoPid) {
    auto r = obd::decode(reply({0x41}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, WrongService) {
    auto r = obd::decode(reply({0x42, 0x0C, 0x1B, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, UnknownPid) {
    auto r = obd::decode(reply({0x41, kUnsupportedPid, 0x1B, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, CoolantTooShort) {
    auto r = obd::decode(reply({0x41, 0x05}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, Coolant) {
    auto r = obd::decode(reply({0x41, 0x05, 0x82}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kCoolant);
    EXPECT_DOUBLE_EQ(r->value, 90);
}

TEST(Decode, CoolantMin) {
    auto r = obd::decode(reply({0x41, 0x05, 0x00}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kCoolant);
    EXPECT_DOUBLE_EQ(r->value, -40);
}

TEST(Decode, RpmTooShort) {
    auto r = obd::decode(reply({0x41, 0x0C, 0x1B}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, RpmTooLong) {
    auto r = obd::decode(reply({0x41, 0x0C, 0x1B, 0x05, 0x00}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, Rpm) {
    auto r = obd::decode(reply({0x41, 0x0C, 0x1B, 0x05}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kRpm);
    EXPECT_DOUBLE_EQ(r->value, 1729.25);
}

TEST(Decode, RpmMax) {
    auto r = obd::decode(reply({0x41, 0x0C, 0xFF, 0xFF}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kRpm);
    EXPECT_DOUBLE_EQ(r->value, 16383.75);
}

TEST(Decode, SpeedTooShort) {
    auto r = obd::decode(reply({0x41, 0x0D}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, Speed) {
    auto r = obd::decode(reply({0x41, 0x0D, 0x3C}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kSpeed);
    EXPECT_DOUBLE_EQ(r->value, 60);
}

TEST(Decode, SpeedMax) {
    auto r = obd::decode(reply({0x41, 0x0D, 0xFF}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kSpeed);
    EXPECT_DOUBLE_EQ(r->value, 255);
}

TEST(MakeRequest, Coolant) {
    EXPECT_EQ(obd::make_request(obd::pid::kCoolant), (obd::Request{0x01, 0x05}));
}

TEST(MakeRequest, Rpm) {
    EXPECT_EQ(obd::make_request(obd::pid::kRpm), (obd::Request{0x01, 0x0C}));
}

TEST(MakeRequest, Speed) {
    EXPECT_EQ(obd::make_request(obd::pid::kSpeed), (obd::Request{0x01, 0x0D}));
}
