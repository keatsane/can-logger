/*
 * an append-only log file. closes itself when it goes out of scope. move-only.
 */
#pragma once

#include <cstdint>
#include <span>

#include "unique_fd.hpp"

class LogFile {
public:
    /**
     * @brief Create a new file at path for writing. Fails if it already exists.
     * @throws std::system_error naming the failed call, with its errno
     */
    explicit LogFile(const char* path);

    /**
     * @brief Append bytes to the file.
     * @throws std::system_error if the write fails or is short
     */
    void write(std::span<const uint8_t> data);

    /**
     * @brief Wait until everything written so far is on the storage device.
     * @throws std::system_error if fsync fails
     */
    void sync();

private:
    UniqueFd fd_;
};
