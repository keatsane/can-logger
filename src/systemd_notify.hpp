/*
 * tells systemd we're alive, for the service watchdog in can-logger.service.
 */
#pragma once

/**
 * @brief Send a state message to systemd, e.g. "WATCHDOG=1".
 * @param state the message to send
 * @return true if it was sent; false if not running under systemd, or the send failed
 */
bool notify_systemd(const char* state);
