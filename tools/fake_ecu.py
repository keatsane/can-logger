#!/usr/bin/env python3
"""Fake engine ECU for bench testing.

Answers standard OBD-II service 0x01 requests on a SocketCAN interface the way
an engine ECU would, with values that change over time like a short drive.
Any request outside the allowed read-only OBD-II services (0x01, 0x02, 0x03,
0x07, 0x09) is flagged, since the logger must never send one.

    python3 tools/fake_ecu.py can_ecu
"""

import argparse
import math
import socket
import struct
import time

FUNCTIONAL_ID = 0x7DF  # "any ecu, answer this"
PHYSICAL_ID = 0x7E0  # engine ecu directly
REPLY_ID = 0x7E8  # engine ecu's replies
PAD = 0x55  # non-zero so a reader that ignores the length byte shows up

READ_ONLY = {0x01, 0x02, 0x03, 0x07, 0x09}
SUPPORTED = (0x05, 0x0C, 0x0D)  # pids listed in the 0x00 bitmap

# struct can_frame: id, len, 3 pad bytes, 8 data bytes
CAN_FRAME = struct.Struct("=IB3x8s")


def sim_state(t):
    """Return (coolant_c, rpm, speed_kmh) at t seconds since start."""
    coolant = 90 - 70 * math.exp(-t / 120)  # warms from 20 to 90 c
    speed = max(0.0, 100 * math.sin(2 * math.pi * t / 120))  # 60 s driving, 60 s stopped
    rpm = 750 + 25 * speed + 30 * math.sin(3 * t)
    return coolant, rpm, speed


def pid_bytes(pid, t):
    """Return the data bytes for a service 0x01 pid, or None if unsupported."""
    coolant, rpm, speed = sim_state(t)
    if pid == 0x00:
        bits = 0
        for p in SUPPORTED:
            bits |= 1 << (32 - p)  # msb is pid 0x01
        return bits.to_bytes(4, "big")
    if pid == 0x05:
        return bytes([round(coolant) + 40])
    if pid == 0x0C:
        return round(rpm * 4).to_bytes(2, "big")
    if pid == 0x0D:
        return bytes([round(speed)])
    return None


def open_socket(iface):
    sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    # only standard-id data frames on the two request ids
    mask = socket.CAN_EFF_FLAG | socket.CAN_RTR_FLAG | socket.CAN_SFF_MASK
    filters = struct.pack("=IIII", FUNCTIONAL_ID, mask, PHYSICAL_ID, mask)
    sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, filters)
    sock.bind((iface,))
    return sock


def send_single_frame(sock, can_id, payload):
    data = bytes([len(payload)]) + payload  # iso-tp single frame: length, then payload
    sock.send(CAN_FRAME.pack(can_id, 8, data.ljust(8, bytes([PAD]))))


def handle(sock, can_id, data, t, counts, quiet):
    """Answer one request frame. Returns nothing; updates counts."""
    req = f"{can_id:03X} {data.hex(' ').upper()}"
    if len(data) < 2 or data[0] >> 4 != 0:
        print(f"{req}  not a single frame, ignored")
        return
    length = data[0] & 0x0F
    if length < 1 or length > 7:
        print(f"{req}  bad length, ignored")
        return
    service = data[1]
    if service not in READ_ONLY:
        counts["forbidden"] += 1
        print(
            f"!! {req}  service 0x{service:02X} is not an allowed obd-ii service"
            " - the logger must never send this"
        )
        return
    if service != 0x01 or length < 2:
        counts["unanswered"] += 1
        print(f"{req}  service 0x{service:02X} not simulated, no reply")
        return
    pid = data[2]
    value = pid_bytes(pid, t)
    if value is None:
        counts["unanswered"] += 1
        print(f"{req}  pid 0x{pid:02X} not supported, no reply")
        return
    if length > 2 and not quiet:
        print(f"{req}  multi-pid request, answering only the first")
    payload = bytes([0x41, pid]) + value
    send_single_frame(sock, REPLY_ID, payload)
    counts["answered"] += 1
    if not quiet:
        print(f"{req}  -> {REPLY_ID:03X} {payload.hex(' ').upper()}")


def main():
    parser = argparse.ArgumentParser(description="fake engine ecu for bench testing")
    parser.add_argument(
        "iface", nargs="?", default="can_ecu", help="socketcan interface (default can_ecu)"
    )
    parser.add_argument("-q", "--quiet", action="store_true", help="only print problems")
    args = parser.parse_args()

    sock = open_socket(args.iface)
    counts = {"answered": 0, "unanswered": 0, "forbidden": 0}
    start = time.monotonic()
    print(
        f"fake ecu on {args.iface}, answering {FUNCTIONAL_ID:03X}/{PHYSICAL_ID:03X}"
        f" on {REPLY_ID:03X}"
    )
    try:
        while True:
            can_id, dlc, data = CAN_FRAME.unpack(sock.recv(CAN_FRAME.size))
            handle(
                sock,
                can_id & socket.CAN_SFF_MASK,
                data[:dlc],
                time.monotonic() - start,
                counts,
                args.quiet,
            )
    except KeyboardInterrupt:
        pass
    print(
        f"\nanswered {counts['answered']}, unanswered {counts['unanswered']}, "
        f"forbidden {counts['forbidden']}"
    )


if __name__ == "__main__":
    main()
