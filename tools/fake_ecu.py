#!/usr/bin/env python3
"""Fake engine ECU for bench testing.

Answers standard OBD-II requests on a SocketCAN interface the way an engine ECU
would: live data that changes over time like a short drive (service 0x01), stored
and pending trouble codes (0x03, 0x07) and a multi-frame VIN (0x09). Any request
outside the allowed read-only OBD-II services (0x01, 0x02, 0x03, 0x07, 0x09) is
flagged, since nothing here may ever send one.

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
FRAME_SIZE = 8  # obd-ii on can only uses full 8-byte frames
FLOW_CONTROL_TIMEOUT_S = 1.0

READ_ONLY = {0x01, 0x02, 0x03, 0x07, 0x09}

VIN = b"WAUTESTBENCH00000"  # 17 characters, not a real car
STORED_DTCS = (0x0301, 0x0420, 0xC100)  # p0301, p0420, u0100: long enough to need 2 frames
PENDING_DTCS = ()

# struct can_frame: id, len, 3 pad bytes, 8 data bytes
CAN_FRAME = struct.Struct("=IB3x8s")


def pid_values(t):
    """Return {pid: data bytes} for every simulated service 0x01 pid, t seconds since start."""
    speed = max(0.0, 100 * math.sin(2 * math.pi * t / 120))  # 60 s driving, 60 s stopped
    rpm = 750 + 25 * speed + 30 * math.sin(3 * t)
    load = 18 + 0.7 * speed
    throttle = 12 + 0.5 * speed
    manifold = 32 + load
    coolant = 90 - 70 * math.exp(-t / 120)  # warms from 20 to 90 c
    intake = 35 - 15 * math.exp(-t / 300)
    short_trim = 3 * math.sin(t / 7)
    long_trim = -2.5
    return {
        0x04: bytes([round(load * 255 / 100)]),
        0x05: bytes([round(coolant) + 40]),
        0x06: bytes([round(short_trim * 128 / 100) + 128]),
        0x07: bytes([round(long_trim * 128 / 100) + 128]),
        0x0B: bytes([round(manifold)]),
        0x0C: round(rpm * 4).to_bytes(2, "big"),
        0x0D: bytes([round(speed)]),
        0x0F: bytes([round(intake) + 40]),
        0x11: bytes([round(throttle * 255 / 100)]),
        0x33: bytes([101]),  # barometric pressure, here so the 0x20 range has something in it
    }


SUPPORTED = tuple(pid_values(0))


def supported_bitmap(base):
    """Return the 4 bytes answering a "which pids" request: msb is pid base + 1, lsb "next range"."""
    bits = 0
    for pid in SUPPORTED:
        if base < pid <= base + 0x20:
            bits |= 1 << (base + 0x20 - pid)
    if any(pid > base + 0x20 for pid in SUPPORTED):
        bits |= 1
    return bits.to_bytes(4, "big")


def pid_bytes(pid, t):
    """Return the data bytes for a service 0x01 pid, or None if unsupported."""
    if pid % 0x20 == 0:
        return supported_bitmap(pid) if pid == 0 or any(p > pid for p in SUPPORTED) else None
    return pid_values(t).get(pid)


def dtc_reply(service, dtcs):
    """Return the reply payload for service 0x03 or 0x07: count, then 2 bytes per code."""
    codes = b"".join(code.to_bytes(2, "big") for code in dtcs)
    return bytes([service + 0x40, len(dtcs)]) + codes


def reply_for(request, t):
    """Return the reply payload for a request payload, or None if it isn't answered."""
    service, rest = request[0], request[1:]
    if service == 0x01 and rest:
        value = pid_bytes(rest[0], t)
        return None if value is None else bytes([0x41, rest[0]]) + value
    if service == 0x03:
        return dtc_reply(service, STORED_DTCS)
    if service == 0x07:
        return dtc_reply(service, PENDING_DTCS)
    if service == 0x09 and rest[:1] == b"\x02":
        return bytes([0x49, 0x02, 0x01]) + VIN  # 0x01: one data item follows
    return None


def pad(data):
    return data.ljust(FRAME_SIZE, bytes([PAD]))


def first_frame(payload):
    """Return the iso-tp first frame: 0x1 and a 12-bit total length, then 6 payload bytes."""
    return bytes([0x10 | len(payload) >> 8, len(payload) & 0xFF]) + payload[:6]


def consecutive_frames(payload):
    """Return the iso-tp frames after the first: 0x2 and a 4-bit counter, then up to 7 bytes."""
    rest = payload[6:]
    chunks = [rest[i : i + 7] for i in range(0, len(rest), 7)]
    return [pad(bytes([0x20 | (i + 1) & 0x0F]) + chunk) for i, chunk in enumerate(chunks)]


def open_socket(iface):
    sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    # only standard-id data frames on the two request ids
    mask = socket.CAN_EFF_FLAG | socket.CAN_RTR_FLAG | socket.CAN_SFF_MASK
    filters = struct.pack("=IIII", FUNCTIONAL_ID, mask, PHYSICAL_ID, mask)
    sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, filters)
    sock.bind((iface,))
    return sock


def send_frame(sock, can_id, data):
    sock.send(CAN_FRAME.pack(can_id, FRAME_SIZE, pad(data)))


def wait_for_flow_control(sock):
    """Return True once the requester says go, False if it never does."""
    sock.settimeout(FLOW_CONTROL_TIMEOUT_S)
    try:
        while True:
            can_id, dlc, data = CAN_FRAME.unpack(sock.recv(CAN_FRAME.size))
            if can_id & socket.CAN_SFF_MASK == PHYSICAL_ID and dlc > 0 and data[0] == 0x30:
                return True  # 0x30: go. the kernel asks for the rest in one go, with no gap
    except TimeoutError:
        return False
    finally:
        sock.settimeout(None)


def send_message(sock, can_id, payload):
    """Send a whole message as iso-tp frames. Returns False if flow control never came."""
    if len(payload) <= 7:
        send_frame(sock, can_id, bytes([len(payload)]) + payload)  # single frame
        return True
    send_frame(sock, can_id, first_frame(payload))
    if not wait_for_flow_control(sock):
        return False
    for frame in consecutive_frames(payload):
        send_frame(sock, can_id, frame)
    return True


def handle(sock, can_id, data, t, counts, quiet):
    """Answer one request frame. Returns nothing; updates counts."""
    req = f"{can_id:03X} {data.hex(' ').upper()}"
    if len(data) != FRAME_SIZE:
        counts["unanswered"] += 1
        print(f"{req}  not {FRAME_SIZE} bytes, a real ecu may ignore this")
        return
    if data[0] >> 4 != 0:
        print(f"{req}  not a single frame, ignored")
        return
    length = data[0] & 0x0F
    if length < 1 or length > 7:
        print(f"{req}  bad length, ignored")
        return
    request = data[1 : 1 + length]
    service = request[0]
    if service not in READ_ONLY:
        counts["forbidden"] += 1
        print(
            f"!! {req}  service 0x{service:02X} is not an allowed obd-ii service"
            " - nothing here may send this"
        )
        return
    payload = reply_for(request, t)
    if payload is None:
        counts["unanswered"] += 1
        print(f"{req}  not simulated, no reply")
        return
    if not send_message(sock, REPLY_ID, payload):
        counts["unanswered"] += 1
        print(f"{req}  no flow control after the first frame")
        return
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
