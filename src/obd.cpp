#include "obd.hpp"

namespace obd {

std::optional<Reading> decode(std::span<const uint8_t> payload) {
    if (payload.size() < 2) return std::nullopt;
    if (payload[0] != kServiceCurrentData + kReplyOffset) return std::nullopt;

    const uint8_t id = payload[1];
    // service, pid, a. only rpm has a second data byte
    if (payload.size() != (id == pid::kRpm ? 4u : 3u)) return std::nullopt;
    const double a = payload[2];  // a double, so the divisions below keep their fractions

    switch (id) {
        case pid::kEngineLoad:
            return Reading{id, a * 100 / 255};
        case pid::kCoolant:
            return Reading{id, a - 40};
        case pid::kShortTermFuelTrim:
            return Reading{id, a * 100 / 128 - 100};
        case pid::kLongTermFuelTrim:
            return Reading{id, a * 100 / 128 - 100};
        case pid::kManifoldPressure:
            return Reading{id, a};
        case pid::kRpm:
            return Reading{id, (a * 256 + payload[3]) / 4};
        case pid::kSpeed:
            return Reading{id, a};
        case pid::kIntakeTemp:
            return Reading{id, a - 40};
        case pid::kThrottle:
            return Reading{id, a * 100 / 255};
        default:
            return std::nullopt;
    }
}

Request make_request(uint8_t pid) {
    return Request{kServiceCurrentData, pid};
}

}  // namespace obd
