"""Tests for tools/car_info.py and tools/fake_ecu.py: each one's replies against the other."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import car_info
import fake_ecu


def reassemble(payload):
    """Join the fake ecu's frames the way a receiver would, checking each one on the way."""
    first = fake_ecu.first_frame(payload)
    total = (first[0] & 0x0F) << 8 | first[1]
    joined = first[2:]
    for i, frame in enumerate(fake_ecu.consecutive_frames(payload)):
        assert len(frame) == fake_ecu.FRAME_SIZE
        assert frame[0] == 0x20 | (i + 1) & 0x0F
        joined += frame[1:]
    return joined[:total]


class Supported(unittest.TestCase):
    def test_every_range_is_walked(self):
        pids = []
        for base in (0x00, 0x20):
            payload = fake_ecu.reply_for(bytes([0x01, base]), 0)
            pids += car_info.decode_supported(base, payload)
        self.assertIn(0x20, pids)  # means the next range exists
        pids.remove(0x20)
        self.assertEqual(pids, sorted(fake_ecu.SUPPORTED))

    def test_last_range_has_no_next(self):
        payload = fake_ecu.reply_for(bytes([0x01, 0x20]), 0)
        self.assertNotIn(0x40, car_info.decode_supported(0x20, payload))

    def test_range_past_the_end_gets_no_reply(self):
        self.assertIsNone(fake_ecu.reply_for(bytes([0x01, 0x40]), 0))


class Dtcs(unittest.TestCase):
    def test_one_of_each_letter(self):
        codes = bytes.fromhex("43 04 0301 4100 8100 C100")
        self.assertEqual(car_info.decode_dtcs(codes), ["P0301", "C0100", "B0100", "U0100"])

    def test_hex_digit_in_a_code(self):
        self.assertEqual(car_info.decode_dtc(0x1A, 0xBC), "P1ABC")

    def test_none_stored(self):
        self.assertEqual(car_info.decode_dtcs(bytes([0x47, 0x00])), [])

    def test_zero_padding_is_not_a_code(self):
        self.assertEqual(car_info.decode_dtcs(bytes.fromhex("43 01 0301 0000")), ["P0301"])

    def test_fake_ecu_stored_codes(self):
        payload = fake_ecu.reply_for(bytes([0x03]), 0)
        self.assertEqual(car_info.decode_dtcs(payload), ["P0301", "P0420", "U0100"])

    def test_fake_ecu_pending_codes(self):
        self.assertEqual(car_info.decode_dtcs(fake_ecu.reply_for(bytes([0x07]), 0)), [])


class Vin(unittest.TestCase):
    def test_fake_ecu_vin(self):
        payload = fake_ecu.reply_for(bytes([0x09, 0x02]), 0)
        self.assertEqual(car_info.decode_vin(payload), fake_ecu.VIN.decode())

    def test_zero_padding_is_stripped(self):
        payload = bytes([0x49, 0x02, 0x01, 0x00, 0x00]) + b"ABC"
        self.assertEqual(car_info.decode_vin(payload), "ABC")


class Matches(unittest.TestCase):
    def test_right_pid(self):
        self.assertTrue(car_info.matches(bytes([0x01, 0x00]), bytes([0x41, 0x00, 1, 2, 3, 4])))

    def test_wrong_pid_is_a_late_reply(self):
        self.assertFalse(car_info.matches(bytes([0x01, 0x20]), bytes([0x41, 0x00, 1, 2, 3, 4])))

    def test_dtc_reply_starts_with_a_count(self):
        self.assertTrue(car_info.matches(bytes([0x03]), bytes([0x43, 0x00])))

    def test_negative_reply(self):
        self.assertFalse(car_info.matches(bytes([0x03]), bytes([0x7F, 0x03, 0x11])))

    def test_empty_reply(self):
        self.assertFalse(car_info.matches(bytes([0x03]), b""))


class Safety(unittest.TestCase):
    # checked before anything goes near a socket, so none is needed here
    def test_forbidden_services_are_refused(self):
        for service in (0x04, 0x10, 0x11, 0x22, 0x27, 0x2E, 0x2F, 0x31, 0x34, 0x3D):
            with self.assertRaises(ValueError):
                car_info.ask(None, bytes([service]))


class Frames(unittest.TestCase):
    def test_vin_splits_and_joins(self):
        payload = fake_ecu.reply_for(bytes([0x09, 0x02]), 0)
        self.assertEqual(len(payload), 20)
        self.assertEqual(reassemble(payload), payload)
        self.assertEqual(len(fake_ecu.consecutive_frames(payload)), 2)

    def test_counter_wraps_after_f(self):
        payload = bytes(range(200))
        frames = fake_ecu.consecutive_frames(payload)
        self.assertEqual(frames[14][0], 0x2F)
        self.assertEqual(frames[15][0], 0x20)
        self.assertEqual(reassemble(payload), payload)


class LiveData(unittest.TestCase):
    def test_every_pid_has_the_right_length(self):
        two_bytes = {0x0C}
        for t in (0, 30, 60, 90, 600):
            for pid, value in fake_ecu.pid_values(t).items():
                self.assertEqual(len(value), 2 if pid in two_bytes else 1, f"pid {pid:02X}")


if __name__ == "__main__":
    unittest.main()
