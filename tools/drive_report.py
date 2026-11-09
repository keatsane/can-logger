"""
Turn a decoded log into a plot and a short summary, for a report that can be made public.

Writes drive.png and report.md to the output directory. The optional car info file adds the
supported pids and trouble codes; its vin is never copied, since the report is meant to be public.

usage: drive_report.py <input file (.csv)> <output directory> [car info file]
"""

import csv
import sys
from pathlib import Path

PIDS = {
    0x04: ("Engine load", "%"),
    0x05: ("Coolant", "C"),
    0x06: ("Short-term fuel trim", "%"),
    0x07: ("Long-term fuel trim", "%"),
    0x0B: ("Manifold pressure", "kPa"),
    0x0C: ("Engine speed", "rpm"),
    0x0D: ("Vehicle speed", "km/h"),
    0x0F: ("Intake air", "C"),
    0x11: ("Throttle", "%"),
}
COOLANT = 0x05
SPEED = 0x0D
WARM_C = 80  # roughly when an engine counts as warmed up

# one y axis per panel, so only things in the same unit share one
PANELS = (
    ("Engine speed (rpm)", (0x0C,)),
    ("Vehicle speed (km/h)", (0x0D,)),
    ("Engine load and throttle (%)", (0x04, 0x11)),
    ("Manifold pressure (kPa)", (0x0B,)),
    ("Temperatures (C)", (0x05, 0x0F)),
    ("Fuel trims (%)", (0x06, 0x07)),
)

CAR_INFO_KEEP = ("pending dtcs:", "stored dtcs:", "supported pids:")

# plot colors
AXIS_COLOR = "#898781"
BACKGROUND = "#fcfcfb"
GRID_COLOR = "#e1e0d9"
LABEL_COLOR = "#52514e"
LINE_COLORS = ("#2a78d6", "#eb6834")
TITLE_COLOR = "#0b0b0b"


def read_log(path):
    """Return ({pid: [(seconds since the first record, value), ...]}, longest gap in seconds)."""
    with open(path, newline="") as f:
        rows = [(int(r["time_us"]), int(r["pid"]), float(r["value"])) for r in csv.DictReader(f)]
    if not rows:
        return {}, 0.0
    start = rows[0][0]
    readings = {}
    for time_us, pid, value in rows:
        readings.setdefault(pid, []).append(((time_us - start) / 1e6, value))
    times = [row[0] for row in rows]
    longest_gap = max((b - a for a, b in zip(times, times[1:])), default=0) / 1e6
    return readings, longest_gap


def duration_s(readings):
    return max((points[-1][0] for points in readings.values()), default=0.0)


def distance_km(points):
    """Return the distance from (seconds, km/h) points, adding up the area under the curve."""
    total = 0.0
    for (t0, v0), (t1, v1) in zip(points, points[1:]):
        total += (v0 + v1) / 2 * (t1 - t0) / 3600
    return total


def warm_up_min(points):
    """Return the minutes until coolant first reaches WARM_C, or None if it never does."""
    for t, value in points:
        if value >= WARM_C:
            return t / 60
    return None


def car_info_lines(path):
    """Return the lines of a car info file that are safe to publish: never the vin."""
    with open(path) as f:
        return [line.strip() for line in f if line.startswith(CAR_INFO_KEEP)]


def report_text(readings, longest_gap, car_info):
    """Return the summary as markdown."""
    records = sum(len(points) for points in readings.values())
    lines = ["# Drive report", ""]
    lines.append(f"- Logged: {duration_s(readings) / 60:.1f} minutes, {records} records")
    lines.append(f"- Longest gap between records: {longest_gap:.2f} s")
    if SPEED in readings:
        lines.append(f"- Distance, from vehicle speed: {distance_km(readings[SPEED]):.1f} km")
    if COOLANT in readings:
        coolant = readings[COOLANT]
        lines.append(
            f"- Coolant: {coolant[0][1]:.0f} C at the start, {coolant[-1][1]:.0f} C at the end"
        )
        # an engine that started warm has no warm-up time to report
        if coolant[0][1] < WARM_C:
            warm = warm_up_min(coolant)
            warm_text = f"{warm:.1f} minutes" if warm is not None else "not reached"
            lines.append(f"- Time to {WARM_C} C coolant: {warm_text}")
    lines += ["", "![Drive plots](drive.png)", ""]
    lines += [
        "| PID | Value | Unit | Records | Min | Mean | Max |",
        "|---|---|---|---|---|---|---|",
    ]
    for pid in sorted(readings):
        name, unit = PIDS.get(pid, ("Unknown", "-"))
        values = [value for _, value in readings[pid]]
        lines.append(
            f"| 0x{pid:02X} | {name} | {unit} | {len(values)} | {min(values):.1f} "
            f"| {sum(values) / len(values):.1f} | {max(values):.1f} |"
        )
    if car_info:
        lines += ["", "## Car info", ""]
        lines += [f"- {line}" for line in car_info]
    return "\n".join(lines) + "\n"


def plot(readings, path):
    """Draw one panel per unit, skipping any panel with nothing to show, and save it to path."""
    import matplotlib

    matplotlib.use("Agg")  # no display needed
    from matplotlib import pyplot as plt

    panels = [p for p in PANELS if any(pid in readings for pid in p[1])]
    if not panels:
        raise ValueError("no known pids to plot")

    fig, axes = plt.subplots(
        len(panels),
        1,
        sharex=True,
        squeeze=False,
        figsize=(10, 1.9 * len(panels) + 0.6),
        layout="constrained",
        facecolor=BACKGROUND,
    )
    for ax, (title, pids) in zip(axes[:, 0], panels):
        ax.set_facecolor(BACKGROUND)
        ax.set_title(title, loc="left", fontsize=10, color=TITLE_COLOR)
        ax.grid(axis="y", color=GRID_COLOR, linewidth=0.6)
        ax.set_axisbelow(True)
        ax.tick_params(colors=AXIS_COLOR, labelsize=8, length=0)
        for side in ("top", "right", "left"):
            ax.spines[side].set_visible(False)
        ax.spines["bottom"].set_color(AXIS_COLOR)
        ax.margins(x=0)
        for pid, color in zip(pids, LINE_COLORS):
            if pid in readings:
                t, values = zip(*readings[pid])
                ax.plot([s / 60 for s in t], values, color=color, linewidth=1.2, label=PIDS[pid][0])
        if len(pids) > 1:
            ax.legend(
                loc="lower right",
                bbox_to_anchor=(1, 1),
                ncols=len(pids),
                frameon=False,
                fontsize=8,
                labelcolor=LABEL_COLOR,
                borderaxespad=0,
            )
    axes[-1, 0].set_xlabel("Minutes since logging started", fontsize=9, color=LABEL_COLOR)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    if not 3 <= len(sys.argv) <= 4:
        sys.exit("usage: drive_report.py <input file (.csv)> <output directory> [car info file]")
    readings, longest_gap = read_log(sys.argv[1])
    if not readings:
        sys.exit(f"no records in {sys.argv[1]}")
    car_info = car_info_lines(sys.argv[3]) if len(sys.argv) == 4 else []
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    plot(readings, out / "drive.png")
    (out / "report.md").write_text(report_text(readings, longest_gap, car_info))
    print(f"wrote {out / 'drive.png'} and {out / 'report.md'}", file=sys.stderr)


if __name__ == "__main__":
    main()
