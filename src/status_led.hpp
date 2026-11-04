/*
 * an led behind a sysfs brightness file. best effort: if the file can't be opened or written
 * nothing happens, so a missing led or a permissions problem can never stop the logging.
 */
#pragma once

#include "unique_fd.hpp"

class StatusLed {
public:
    /**
     * @brief Open the led's brightness file. Never throws; a failure just makes set() do nothing.
     * @param path e.g. "/sys/class/leds/ACT/brightness"
     */
    explicit StatusLed(const char* path);

    /**
     * @brief Turn the led on or off.
     * @param on true for on
     * @return true if the value was written
     */
    bool set(bool on);

private:
    UniqueFd fd_;
};
