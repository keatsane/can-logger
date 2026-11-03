/*
 * an iso-tp socket on one can interface: requests go out on one id, replies come back on another.
 * closes itself when it goes out of scope. move-only, so exactly one object owns the socket.
 */
#pragma once

#include <linux/can.h>

#include <cstdint>
#include <optional>
#include <span>

#include "unique_fd.hpp"

class IsoTpSocket {
public:
    /**
     * @brief Open an iso-tp socket on ifname that sends on tx_id and only receives rx_id.
     * @param ifname interface name, e.g. "can_obd"
     * @param tx_id id our requests go out on
     * @param rx_id id the replies come back on
     * @throws std::system_error naming the failed call, with its errno
     */
    IsoTpSocket(const char* ifname, canid_t tx_id, canid_t rx_id);

    /**
     * @brief Send one whole message; the kernel splits it into frames if needed.
     * @param payload the bytes to send, e.g. 01 0c
     * @throws std::system_error if the write fails or is short
     */
    void send(std::span<const uint8_t> payload);

    /**
     * @brief Wait for one whole message.
     * @param buf where the message is stored; the caller owns it
     * @param timeout_ms how long to wait
     * @return the part of buf holding the message, or std::nullopt on timeout
     * @throws std::system_error if poll or read fails
     */
    std::optional<std::span<const uint8_t>> receive(std::span<uint8_t> buf, int timeout_ms);

private:
    UniqueFd fd_;
};
