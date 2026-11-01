#include "obd.hpp"

namespace obd {

std::optional<Reading> decode(const can_frame& frame) {
    if (frame.len != 8) return std::nullopt;
    if (frame.data[1] != kServiceCurrentData + kReplyOffset) return std::nullopt;
    if (frame.data[2] != pid::kRpm || frame.data[0] < 4) return std::nullopt;

    return Reading{frame.data[2], (frame.data[3] * 256 + frame.data[4]) / 4.0};
}

}  // namespace obd
