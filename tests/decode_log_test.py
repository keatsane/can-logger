"""Tests for tools/decode_log.py: hand it damaged logs and check what it reports."""

import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

DECODER = Path(__file__).resolve().parent.parent / "tools" / "decode_log.py"
SIZE = 28
VALUE_OFFSET = 16


def record(seq, pid=0x0C, value=1000.0, time_us=1_000_000, magic=b"CL", version=1):
    head = struct.pack("<2sBBIQd", magic, version, pid, seq, time_us, value)
    return head + struct.pack("<I", zlib.crc32(head))


def log(count=5):
    return bytearray(b"".join(record(seq) for seq in range(count)))


def decode(data):
    """Run the decoder on data, return (stdout lines, stderr lines)."""
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        f.write(data)
        f.flush()
        result = subprocess.run(
            [sys.executable, str(DECODER), f.name], capture_output=True, text=True, check=True
        )
    return result.stdout.splitlines(), result.stderr.splitlines()


class DecodeLog(unittest.TestCase):
    def test_clean_log(self):
        out, err = decode(log())
        self.assertEqual(out[0], "seq,time_us,pid,value")
        self.assertEqual(out[1], "0,1000000,12,1000.0")
        self.assertEqual(len(out), 6)
        self.assertEqual(err, ["5 good, 0 bad, 0 missing"])

    def test_empty_log(self):
        out, err = decode(b"")
        self.assertEqual(out, ["seq,time_us,pid,value"])
        self.assertEqual(err, ["0 good, 0 bad, 0 missing"])

    # the same record the c++ test checks, so the two languages can't drift apart
    def test_matches_the_cpp_encoder(self):
        expected_crc = 0xDFB912C9
        self.assertEqual(
            record(7, pid=0x05, value=90.0, time_us=1000)[-4:], expected_crc.to_bytes(4, "little")
        )

    def test_flipped_byte(self):
        data = log()
        data[2 * SIZE + VALUE_OFFSET] ^= 0xFF
        _, err = decode(data)
        self.assertIn("bad record at byte 56", err)
        self.assertIn("expected seq 2 but got 3", err)
        self.assertEqual(err[-1], "4 good, 1 bad, 1 missing")

    def test_first_record_damaged(self):
        data = log()
        data[VALUE_OFFSET] ^= 0xFF
        _, err = decode(data)
        self.assertIn("bad record at byte 0", err)
        self.assertEqual(err[-1], "4 good, 1 bad, 1 missing")

    def test_last_record_damaged(self):
        data = log()
        data[4 * SIZE + VALUE_OFFSET] ^= 0xFF
        _, err = decode(data)
        self.assertEqual(err, ["bad record at byte 112", "4 good, 1 bad, 0 missing"])

    def test_record_removed(self):
        data = log()
        del data[2 * SIZE : 3 * SIZE]
        _, err = decode(data)
        self.assertEqual(err, ["expected seq 2 but got 3", "4 good, 0 bad, 1 missing"])

    def test_one_byte_removed_resyncs(self):
        data = log()
        del data[2 * SIZE + VALUE_OFFSET]
        _, err = decode(data)
        self.assertEqual(err[-1], "4 good, 1 bad, 1 missing")

    def test_cut_off_at_the_end(self):
        _, err = decode(log()[:-14])
        self.assertEqual(err, ["partial record found at byte 112", "4 good, 0 bad, 0 missing"])

    # a damaged record with "CL" inside its value: the search lands there, and the crc rejects it
    def test_false_marker_is_rejected(self):
        fake = struct.unpack("<d", b"\x00\x00CL\x00\x00\x00\x00")[0]
        data = bytearray(record(0) + record(1, value=fake) + record(2))
        data[SIZE + 3] ^= 0xFF
        _, err = decode(data)
        self.assertEqual(err[-1], "2 good, 2 bad, 1 missing")

    # the crc is right in both, so only the marker and version checks can catch them
    def test_wrong_marker_is_rejected(self):
        _, err = decode(record(0) + record(1, magic=b"XX") + record(2))
        self.assertIn("bad record at byte 28", err)
        self.assertEqual(err[-1], "2 good, 1 bad, 1 missing")

    def test_unknown_version_is_rejected(self):
        _, err = decode(record(0) + record(1, version=2) + record(2))
        self.assertIn("bad record at byte 28", err)
        self.assertEqual(err[-1], "2 good, 1 bad, 1 missing")


if __name__ == "__main__":
    unittest.main()
