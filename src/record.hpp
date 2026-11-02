/*
 * the on-disk log record: fixed size, little-endian, with a crc32 at the end so a reader can
 * tell a damaged record from a good one, and a sequence number so it can tell one is missing.
 *
 *   offset  size  field
 *        0     2  magic "CL" (0x43 0x4c), marks the start of a record
 *        2     1  format version (kVersion)
 *        3     1  pid
 *        4     4  sequence number, counts up from 0 each session
 *        8     8  time the reply arrived, microseconds since the unix epoch
 *       16     8  decoded value, ieee-754 double
 *       24     4  crc32 of bytes 0-23
 *
 * no sockets or files here, so it unit tests anywhere.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "obd.hpp"

namespace record {

constexpr std::size_t kSize = 28;
constexpr uint8_t kVersion = 1;

using Bytes = std::array<uint8_t, kSize>;

/**
 * @brief Standard crc-32 (ieee 802.3, the same one zlib and python's zlib.crc32 use).
 * @param data the bytes to check
 * @return the crc
 */
uint32_t crc32(std::span<const uint8_t> data);

/**
 * @brief Lay out one record.
 * @param seq sequence number
 * @param time_us when the reply arrived, microseconds since the unix epoch
 * @param reading the decoded pid and value
 * @return the 28 bytes to write
 */
Bytes encode(uint32_t seq, uint64_t time_us, const obd::Reading& reading);

}  // namespace record
