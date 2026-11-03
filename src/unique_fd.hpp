/*
 * owns one file descriptor and closes it when it goes out of scope. move-only.
 * also the errno helpers shared by the classes that use it.
 */
#pragma once

#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <span>
#include <system_error>

class UniqueFd {
public:
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() {
        if (fd_ >= 0) close(fd_);
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }  // the raw number, for system calls

private:
    int fd_ = -1;
};

// throw errno as an exception, naming the call that failed
[[noreturn]] inline void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

// write all of data or throw. a short write counts as an error
inline void write_all(int fd, std::span<const uint8_t> data) {
    ssize_t n = ::write(fd, data.data(), data.size());
    if (n != static_cast<ssize_t>(data.size())) {
        if (n >= 0) errno = EIO;
        throw_errno("write");
    }
}
