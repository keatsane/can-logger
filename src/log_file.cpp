
#include "log_file.hpp"

#include <fcntl.h>
#include <unistd.h>

LogFile::LogFile(const char* path)
    : fd_(open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644)) {
    if (fd_.get() < 0) throw_errno(path);
}

void LogFile::write(std::span<const uint8_t> data) {
    write_all(fd_.get(), data);
}

void LogFile::sync() {
    if (fsync(fd_.get()) < 0) throw_errno("fsync");
}
