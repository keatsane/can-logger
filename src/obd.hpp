/*
 * obd-ii service 0x01 requests and replies over can (iso 15765-4, 11-bit ids).
 * no sockets here, so everything in this file can be unit tested on any machine.
 */
#pragma once

#include <linux/can.h>

#include <cstdint>
#include <optional>

namespace obd {

constexpr canid_t kRequestId = 0x7DF;      // functional request: any ecu answers
constexpr canid_t kEngineReplyId = 0x7E8;  // engine ecu's replies

constexpr uint8_t kServiceCurrentData = 0x01;
constexpr uint8_t kReplyOffset = 0x40;  // positive reply = service + 0x40

namespace pid {
constexpr uint8_t kSupported = 0x00;
constexpr uint8_t kCoolant = 0x05;
constexpr uint8_t kRpm = 0x0C;
constexpr uint8_t kSpeed = 0x0D;
}  // namespace pid

/// one decoded value from a reply
struct Reading {
    uint8_t pid;
    double value;
};

/**
 * @brief Decode a service 0x01 reply frame.
 * @param frame a received frame, expected on kEngineReplyId
 * @return the reading, or std::nullopt if the frame isn't a valid reply for a supported pid
 */
std::optional<Reading> decode(const can_frame& frame);

}  // namespace obd
