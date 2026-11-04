#include "status_led.hpp"

#include <fcntl.h>
#include <unistd.h>

StatusLed::StatusLed(const char* path) : fd_(open(path, O_WRONLY | O_CLOEXEC)) {}

bool StatusLed::set(bool on) {
    if (fd_.get() < 0) return false;
    // the file holds one value, so always write at the start of it
    return pwrite(fd_.get(), on ? "1" : "0", 1, 0) == 1;
}
