#include <cstdint>
#include <cstdio>
#include <system_error>

#include "isotp_socket.hpp"
#include "obd.hpp"

constexpr int kTimeoutMs = 100;

int main() {
    try {
        IsoTpSocket sock("can_obd", obd::kEngineRequestId, obd::kEngineReplyId);
        sock.send(obd::make_request(obd::pid::kRpm));

        uint8_t buf[64];
        auto reply = sock.receive(buf, kTimeoutMs);
        if (!reply) {
            std::fprintf(stderr, "no reply within %d ms\n", kTimeoutMs);
            return 1;
        }

        auto reading = obd::decode(*reply);
        if (!reading || reading->pid != obd::pid::kRpm) {
            std::fprintf(stderr, "unexpected reply\n");
            return 1;
        }

        std::printf("rpm: %.2f\n", reading->value);
        return 0;
    } catch (const std::system_error& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
