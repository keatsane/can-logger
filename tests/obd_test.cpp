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

TEST(Decode, EngineLoadTooShort) {
    auto r = obd::decode(reply({0x41, 0x04}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, EngineLoad) {
    auto r = obd::decode(reply({0x41, 0x04, 0x33}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kEngineLoad);
    EXPECT_DOUBLE_EQ(r->value, 20);
}

TEST(Decode, EngineLoadMax) {
    auto r = obd::decode(reply({0x41, 0x04, 0xFF}));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->value, 100);
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

TEST(Decode, ShortTermFuelTrimTooShort) {
    auto r = obd::decode(reply({0x41, 0x06}));
    EXPECT_FALSE(r.has_value());
}

// 0x80 is the middle of the byte: no correction
TEST(Decode, ShortTermFuelTrimZero) {
    auto r = obd::decode(reply({0x41, 0x06, 0x80}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kShortTermFuelTrim);
    EXPECT_DOUBLE_EQ(r->value, 0);
}

TEST(Decode, ShortTermFuelTrimMin) {
    auto r = obd::decode(reply({0x41, 0x06, 0x00}));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->value, -100);
}

TEST(Decode, ShortTermFuelTrimMax) {
    auto r = obd::decode(reply({0x41, 0x06, 0xFF}));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->value, 99.21875);
}

TEST(Decode, LongTermFuelTrimTooShort) {
    auto r = obd::decode(reply({0x41, 0x07}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, LongTermFuelTrim) {
    auto r = obd::decode(reply({0x41, 0x07, 0xC0}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kLongTermFuelTrim);
    EXPECT_DOUBLE_EQ(r->value, 50);
}

TEST(Decode, ManifoldPressureTooShort) {
    auto r = obd::decode(reply({0x41, 0x0B}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, ManifoldPressure) {
    auto r = obd::decode(reply({0x41, 0x0B, 0x65}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kManifoldPressure);
    EXPECT_DOUBLE_EQ(r->value, 101);
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

TEST(Decode, IntakeTempTooShort) {
    auto r = obd::decode(reply({0x41, 0x0F}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, IntakeTemp) {
    auto r = obd::decode(reply({0x41, 0x0F, 0x3C}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kIntakeTemp);
    EXPECT_DOUBLE_EQ(r->value, 20);
}

TEST(Decode, ThrottleTooShort) {
    auto r = obd::decode(reply({0x41, 0x11}));
    EXPECT_FALSE(r.has_value());
}

TEST(Decode, Throttle) {
    auto r = obd::decode(reply({0x41, 0x11, 0x33}));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->pid, obd::pid::kThrottle);
    EXPECT_DOUBLE_EQ(r->value, 20);
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
