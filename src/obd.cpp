#include "obd.hpp"

namespace obd {

std::optional<Reading> decode(const can_frame& frame) {
    if (frame.len != 8 || frame.data[0] < 4 || frame.data[1] != 0x41 || frame.data[2] != 0x0C)
        return std::nullopt;

    return Reading{frame.data[2], (frame.data[3] * 256 + frame.data[4]) / 4.0};
}

}  // namespace obd
