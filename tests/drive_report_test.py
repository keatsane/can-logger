"""Tests for tools/drive_report.py: the numbers it reports, and that the vin stays out."""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import drive_report


def write_csv(folder, rows):
    """Write (time_us, pid, value) rows as the decoder would, and return the path."""
    path = Path(folder) / "log.csv"
    lines = ["seq,time_us,pid,value"]
    lines += [f"{seq},{t},{pid},{value}" for seq, (t, pid, value) in enumerate(rows)]
    path.write_text("\n".join(lines) + "\n")
    return path


class ReadLog(unittest.TestCase):
    def test_times_start_at_zero_and_group_by_pid(self):
        rows = [(5_000_000, 0x0C, 800.0), (6_000_000, 0x0D, 10.0), (7_000_000, 0x0C, 900.0)]
        with tempfile.TemporaryDirectory() as folder:
            readings, gap = drive_report.read_log(write_csv(folder, rows))
        self.assertEqual(readings[0x0C], [(0.0, 800.0), (2.0, 900.0)])
        self.assertEqual(readings[0x0D], [(1.0, 10.0)])
        self.assertEqual(gap, 1.0)

    def test_empty_log(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(drive_report.read_log(write_csv(folder, [])), ({}, 0.0))

    def test_longest_gap_crosses_pids(self):
        rows = [(0, 0x0C, 1.0), (1_000_000, 0x0D, 1.0), (4_500_000, 0x0C, 1.0)]
        with tempfile.TemporaryDirectory() as folder:
            _, gap = drive_report.read_log(write_csv(folder, rows))
        self.assertEqual(gap, 3.5)


class Numbers(unittest.TestCase):
    def test_distance_at_steady_speed(self):
        self.assertAlmostEqual(drive_report.distance_km([(0, 100.0), (360, 100.0)]), 10.0)

    def test_distance_while_speeding_up(self):
        self.assertAlmostEqual(drive_report.distance_km([(0, 0.0), (360, 100.0)]), 5.0)

    def test_distance_of_one_point(self):
        self.assertEqual(drive_report.distance_km([(0, 100.0)]), 0.0)

    def test_warm_up(self):
        points = [(0, 20.0), (60, 79.0), (120, 80.0), (180, 90.0)]
        self.assertEqual(drive_report.warm_up_min(points), 2.0)

    def test_warm_up_never_reached(self):
        self.assertIsNone(drive_report.warm_up_min([(0, 20.0), (60, 40.0)]))


class Report(unittest.TestCase):
    def test_vin_is_never_copied(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "car-info.txt"
            path.write_text(
                "supported pids: 04 05\nvin: WAUTESTBENCH00000\n"
                "stored dtcs: P0301\npending dtcs: none\n"
            )
            lines = drive_report.car_info_lines(path)
        self.assertEqual(
            lines, ["supported pids: 04 05", "stored dtcs: P0301", "pending dtcs: none"]
        )
        self.assertNotIn("WAUTESTBENCH", drive_report.report_text({}, 0.0, lines))

    def test_summary_values(self):
        readings = {0x05: [(0, 20.0), (600, 90.0)], 0x0D: [(0, 100.0), (360, 100.0)]}
        text = drive_report.report_text(readings, 1.5, [])
        self.assertIn("10.0 minutes, 4 records", text)
        self.assertIn("Longest gap between records: 1.50 s", text)
        self.assertIn("Distance, from vehicle speed: 10.0 km", text)
        self.assertIn("20 C at the start, 90 C at the end", text)
        self.assertIn("Time to 80 C coolant: 10.0 minutes", text)
        self.assertIn("| 0x05 | Coolant | C | 2 | 20.0 | 55.0 | 90.0 |", text)

    def test_warm_start_has_no_warm_up_time(self):
        text = drive_report.report_text({0x05: [(0, 102.0), (600, 108.0)]}, 1.0, [])
        self.assertIn("102 C at the start, 108 C at the end", text)
        self.assertNotIn("Time to", text)

    def test_missing_pids_are_left_out(self):
        text = drive_report.report_text({0x0C: [(0, 800.0), (1, 900.0)]}, 1.0, [])
        self.assertNotIn("Distance", text)
        self.assertNotIn("Coolant", text)


@unittest.skipUnless(importlib.util.find_spec("matplotlib"), "matplotlib not installed")
class Plot(unittest.TestCase):
    def test_writes_a_png(self):
        readings = {0x0C: [(0, 800.0), (60, 900.0)]}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "drive.png"
            drive_report.plot(readings, path)
            self.assertEqual(path.read_bytes()[:4], b"\x89PNG")

    def test_nothing_known_to_plot(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):
                drive_report.plot({0xFF: [(0, 1.0)]}, Path(folder) / "drive.png")


if __name__ == "__main__":
    unittest.main()
