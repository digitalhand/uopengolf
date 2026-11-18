# SPDX-License-Identifier: MIT
"""Preserve recorded launch inputs for replay, without treating old engine results as truth."""
import argparse
import csv
import json
import math
from pathlib import Path
import shutil

try:
    from .square_import import SHOTS_FIELDS
except ImportError:
    from square_import import SHOTS_FIELDS


def import_recordings(source, destination, session_prefix="session_20261113"):
    destination.mkdir(parents=True, exist_ok=True)
    rows = []
    for path in sorted(source.glob(f"{session_prefix}*/*.json")):
        record = json.loads(path.read_text(encoding="utf-8-sig"))
        ball = record["BallData"]
        back, side = ball.get("BackSpin"), ball.get("SideSpin")
        total, axis = ball.get("TotalSpin"), ball.get("SpinAxis")
        if back is None or side is None:
            if total is None or axis is None:
                raise ValueError(f"Incomplete spin: {path}")
            back = total * math.cos(math.radians(axis)) if back is None else back
            side = total * math.sin(math.radians(axis)) if side is None else side
        row = dict.fromkeys(SHOTS_FIELDS, "")
        # The club selected in the game is kept for reference only; it is not reliable and nothing reads it.
        row.update(shot_name=f"{path.parent.name}_{path.stem}", reported_club=record.get("UOpenGolf", {}).get("Club", ""),
                   speed_mph=ball["Speed"], vla_deg=ball["VLA"], hla_deg=ball["HLA"],
                   backspin_rpm=back, sidespin_rpm=side,
                   total_spin_rpm=total if total is not None else math.hypot(back, side),
                   spin_axis_deg=axis if axis is not None else math.degrees(math.atan2(side, back)))
        rows.append(row)
        preserved = destination / "recordings" / path.parent.name / path.name
        preserved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, preserved)
    if not rows:
        raise ValueError("No recordings found")
    with (destination / "shots.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=SHOTS_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    return len(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--session-prefix", default="session_20261113")
    args = parser.parse_args()
    print(f"Imported {import_recordings(args.source, args.destination, args.session_prefix)} recordings")
