"""
Ask the engine ecu once for its supported pids, vin and trouble codes.

Only sends read-only obd-ii requests, a few per second. Don't run it while the logger is
running: both would be waiting for replies on the same ids.

usage: car_info.py [interface] | tee ~/logs/car-info.txt
"""

import socket
import struct
import sys
import time

REQUEST_ID = 0x7E0  # engine ecu directly
REPLY_ID = 0x7E8  # engine ecu's replies
ALLOWED_SERVICES = {0x01, 0x03, 0x07, 0x09}
GAP_S = 0.2  # same spacing as the logger
TIMEOUT_S = 1.0
MAX_MESSAGE = 4095  # longest iso-tp message

SERVICE_CURRENT_DATA = 0x01
SERVICE_STORED_DTCS = 0x03
SERVICE_PENDING_DTCS = 0x07
SERVICE_VEHICLE_INFO = 0x09
INFO_VIN = 0x02
NEGATIVE_REPLY = 0x7F
STILL_WORKING = 0x78  # negative reply code: wait, the real answer is coming

# from linux/can/isotp.h
CAN_ISOTP_OPTS = 1
CAN_ISOTP_TX_PADDING = 0x004


def decode_supported(base, payload):
    """Return the pids set in a "which pids" reply: 41 base, then 4 bytes, msb is base + 1."""
    bits = int.from_bytes(payload[2:6], "big")
    return [base + i + 1 for i in range(32) if bits >> (31 - i) & 1]


def decode_dtc(high, low):
    """Return one trouble code as text, e.g. 0x03 0x01 -> P0301."""
    letter = "PCBU"[high >> 6]
    return f"{letter}{high >> 4 & 0x3}{high & 0xF:X}{low:02X}"


def decode_dtcs(payload):
    """Return the codes in a service 0x03 or 0x07 reply: 43 count, then 2 bytes per code."""
    codes = payload[2:]
    pairs = zip(codes[0::2], codes[1::2])
    return [decode_dtc(high, low) for high, low in pairs if high or low]


def decode_vin(payload):
    """Return the vin from a service 0x09 reply: 49 02 01, then ascii, sometimes zero-padded."""
    return payload[3:].decode("ascii", "replace").strip("\x00 ")


def matches(request, reply):
    """Return True if reply is the positive answer to request."""
    if not reply or reply[0] != request[0] + 0x40:
        return False
    # service 0x01 and 0x09 replies repeat the pid; 0x03 and 0x07 replies start with a count
    return request[0] in (SERVICE_STORED_DTCS, SERVICE_PENDING_DTCS) or reply[1:2] == request[1:2]


def ask(sock, request):
    """Send one request. Returns its reply payload, or None if refused or unanswered."""
    if request[0] not in ALLOWED_SERVICES:
        raise ValueError(f"service 0x{request[0]:02X} is not allowed here")
    time.sleep(GAP_S)
    sock.send(request)
    while True:
        try:
            reply = sock.recv(MAX_MESSAGE)
        except TimeoutError:
            return None
        if matches(request, reply):
            return reply
        refused = len(reply) >= 3 and reply[0] == NEGATIVE_REPLY and reply[1] == request[0]
        if refused and reply[2] != STILL_WORKING:
            print(f"refused: {request.hex(' ')} -> {reply.hex(' ')}", file=sys.stderr)
            return None


def read_supported(sock):
    """Walk the "which pids" ranges. Returns the pids, or None if the first request got no reply."""
    pids = []
    for base in range(0x00, 0x100, 0x20):
        reply = ask(sock, bytes([SERVICE_CURRENT_DATA, base]))
        if reply is None:
            return None if base == 0 else pids
        found = decode_supported(base, reply)
        pids += [pid for pid in found if pid % 0x20]  # the rest only mean "next range exists"
        if base + 0x20 not in found:
            break
    return pids


def print_dtcs(sock, name, service):
    reply = ask(sock, bytes([service]))
    if reply is None:
        print(f"{name} dtcs: no reply")
    else:
        print(f"{name} dtcs: {' '.join(decode_dtcs(reply)) or 'none'}")


def open_socket(iface):
    sock = socket.socket(socket.AF_CAN, socket.SOCK_DGRAM, socket.CAN_ISOTP)
    # full 8-byte frames, like the logger. flags, tx time, then four single-byte options
    opts = struct.pack("=IIBBBB", CAN_ISOTP_TX_PADDING, 0, 0, 0, 0, 0)
    sock.setsockopt(socket.SOL_CAN_BASE + socket.CAN_ISOTP, CAN_ISOTP_OPTS, opts)
    sock.bind((iface, REPLY_ID, REQUEST_ID))
    sock.settimeout(TIMEOUT_S)
    return sock


def main():
    if len(sys.argv) > 2:
        sys.exit("usage: car_info.py [interface] | tee ~/logs/car-info.txt")
    iface = sys.argv[1] if len(sys.argv) == 2 else "can_obd"
    try:
        with open_socket(iface) as sock:
            pids = read_supported(sock)
            if pids is None:
                sys.exit("no reply from the engine ecu")
            print(f"supported pids: {' '.join(f'{pid:02X}' for pid in pids)}")
            reply = ask(sock, bytes([SERVICE_VEHICLE_INFO, INFO_VIN]))
            print(f"vin: {decode_vin(reply) if reply else 'no reply'}")
            print_dtcs(sock, "stored", SERVICE_STORED_DTCS)
            print_dtcs(sock, "pending", SERVICE_PENDING_DTCS)
    except OSError as e:
        sys.exit(f"car_info: {e}")


if __name__ == "__main__":
    main()
