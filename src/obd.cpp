#include "obd.hpp"

namespace obd {

std::optional<Reading> decode(std::span<const uint8_t> payload) {
    if (payload.size() < 2) return std::nullopt;
    if (payload[0] != kServiceCurrentData + kReplyOffset) return std::nullopt;

    switch (payload[1]) {
        case pid::kCoolant:
            if (payload.size() != 3) return std::nullopt;  // service, pid, a
            return Reading{pid::kCoolant, payload[2] - 40.0};
        case pid::kRpm:
            if (payload.size() != 4) return std::nullopt;  // service, pid, a, b
            return Reading{pid::kRpm, (payload[2] * 256 + payload[3]) / 4.0};
        case pid::kSpeed:
            if (payload.size() != 3) return std::nullopt;  // service, pid, a
            return Reading{pid::kSpeed, static_cast<double>(payload[2])};
        default:
            return std::nullopt;
    }
}

Request make_request(uint8_t pid) {
    return Request{kServiceCurrentData, pid};
}

}  // namespace obd
