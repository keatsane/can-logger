"""
Decode a can-logger log file: csv on stdout, problems and a summary on stderr.

usage: decode_log.py <input file (.bin)> > <output file (.csv)>
"""

import struct
import sys
import zlib

RECORD_FORMAT = "<2s2BIQdI"
RECORD_SIZE = struct.calcsize(RECORD_FORMAT)
CRC_BYTES = 24  # crc covers first 24 bytes


def decode_log(path):
    with open(path, "rb") as f:
        data = f.read()
    pos = good = bad = missing = expected = 0
    print("seq,time_us,pid,value")
    while pos < len(data):
        record = data[pos : pos + RECORD_SIZE]
        if len(record) < RECORD_SIZE:
            print(f"partial record found at byte {pos}", file=sys.stderr)
            break
        magic, version, pid, seq, time_us, value, crc = struct.unpack(RECORD_FORMAT, record)
        if zlib.crc32(record[:CRC_BYTES]) != crc:
            bad += 1
            print(f"bad record at byte {pos}", file=sys.stderr)
            pos = data.find(b"CL", pos + 1)
            if pos == -1:
                break
            continue
        if seq != expected:
            print(f"expected seq {expected} but got {seq}", file=sys.stderr)
            missing += seq - expected
        expected = seq + 1
        good += 1
        pos += RECORD_SIZE
        print(f"{seq},{time_us},{pid},{value}")
    print(f"{good} good, {bad} bad, {missing} missing", file=sys.stderr)


if __name__ == "__main__":
    decode_log(sys.argv[1])
