#include "record.hpp"

#include <bit>

namespace record {

namespace {

constexpr uint32_t kCrcPoly = 0xEDB88320;  // crc-32 polynomial, bit-reversed

// write the low n bytes of v at b[at], lowest byte first
void put_le(Bytes& b, std::size_t at, uint64_t v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        b[at + i] = static_cast<uint8_t>(v >> (8 * i));
}

}  // namespace

uint32_t crc32(std::span<const uint8_t> data) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1)
                crc = (crc >> 1) ^ kCrcPoly;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

Bytes encode(uint32_t seq, uint64_t time_us, const obd::Reading& reading) {
    Bytes b{};
    b[0] = 0x43;  // 'C'
    b[1] = 0x4C;  // 'L'
    b[2] = kVersion;
    b[3] = reading.pid;
    put_le(b, 4, seq, 4);
    put_le(b, 8, time_us, 8);
    put_le(b, 16, std::bit_cast<uint64_t>(reading.value), 8);
    put_le(b, 24, crc32(std::span<const uint8_t>(b.data(), 24)), 4);
    return b;
}

}  // namespace record
