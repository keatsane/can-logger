/*
 * obd-ii service 0x01 requests and replies (iso 15765-4, 11-bit ids).
 * works on iso-tp payloads: the kernel's iso-tp socket adds and strips the length byte and
 * padding, and splits and joins long messages. no sockets here, so it unit tests anywhere.
 */
#pragma once

#include <linux/can.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace obd {

constexpr canid_t kEngineRequestId = 0x7E0;  // physical request to the engine ecu
constexpr canid_t kEngineReplyId = 0x7E8;    // engine ecu's replies

constexpr uint8_t kServiceCurrentData = 0x01;
constexpr uint8_t kReplyOffset = 0x40;  // positive reply = service + 0x40

namespace pid {
constexpr uint8_t kEngineLoad = 0x04;
constexpr uint8_t kCoolant = 0x05;
constexpr uint8_t kShortTermFuelTrim = 0x06;
constexpr uint8_t kLongTermFuelTrim = 0x07;
constexpr uint8_t kManifoldPressure = 0x0B;
constexpr uint8_t kRpm = 0x0C;
constexpr uint8_t kSpeed = 0x0D;
constexpr uint8_t kIntakeTemp = 0x0F;
constexpr uint8_t kThrottle = 0x11;
}  // namespace pid

/// one decoded value from a reply
struct Reading {
    uint8_t pid;
    double value;
};

/// a service 0x01 request payload: service, pid
using Request = std::array<uint8_t, 2>;

/**
 * @brief Decode a service 0x01 reply payload.
 * @param payload the bytes an iso-tp read returned, e.g. 41 0c 1b 05
 * @return the reading, or std::nullopt if it isn't a valid reply for a supported pid
 */
std::optional<Reading> decode(std::span<const uint8_t> payload);

/**
 * @brief Build a service 0x01 request payload for one pid.
 * @param pid the parameter id to ask for
 * @return the two bytes to write to an iso-tp socket
 */
Request make_request(uint8_t pid);

}  // namespace obd
