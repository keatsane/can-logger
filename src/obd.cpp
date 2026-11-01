#include "obd.hpp"

namespace obd {

std::optional<Reading> decode(const can_frame& frame) {
    if (frame.len != 8) return std::nullopt;
    if (frame.data[1] != kServiceCurrentData + kReplyOffset) return std::nullopt;

    switch (frame.data[2]) {
        case pid::kCoolant:
            if (frame.data[0] != 3) return std::nullopt;  // service, pid, a
            return Reading{pid::kCoolant, frame.data[3] - 40.0};
        case pid::kRpm:
            if (frame.data[0] != 4) return std::nullopt;  // service, pid, a, b
            return Reading{pid::kRpm, (frame.data[3] * 256 + frame.data[4]) / 4.0};
        case pid::kSpeed:
            if (frame.data[0] != 3) return std::nullopt;  // service, pid, a
            return Reading{pid::kSpeed, static_cast<double>(frame.data[3])};
        default:
            return std::nullopt;
    }
}

can_frame make_request(uint8_t pid) {
    can_frame f{};
    f.can_id = kRequestId;
    f.len = 8;
    f.data[0] = 2;  // service, pid
    f.data[1] = kServiceCurrentData;
    f.data[2] = pid;
    return f;
}

}  // namespace obd
