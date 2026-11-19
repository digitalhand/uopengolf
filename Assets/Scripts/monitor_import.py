# SPDX-License-Identifier: MIT
"""Import a launch-monitor app's shot export into the local shot library as a new session.

The export is one CSV per range session: a "Dates" line, a header and one row per shot, grouped by
club with "Average" and "Deviation" rows after each club. Directions carry an L or R prefix (L is
negative, left), and the column labelled "Apex(yd)" holds feet.

The Club column is the club selected in the app, not a measurement (the export has ClubSpeed and
SmashFactor as 0), and it is often wrong. It is kept as reported_club and in the shot names, which
index the rows within each label; nothing compares or groups shots by it.

Usage, from the repository root:

    python Assets/Scripts/monitor_import.py <export.csv> [--session NAME]

This copies the export unchanged into <local data>/shot_library/<session>/export.csv and writes the
normalized shots.csv that UOpenGolf.Physics.ShotLibrary replays. The local data folder is outside the
repository (data_paths.py, Assets/Data/README.md). The session name defaults to export_<yyyymmdd>
from the export's date line.
"""
import argparse
import csv
from datetime import datetime
from pathlib import Path
import re
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from data_paths import local_library_dir  # noqa: E402  (run as a script or imported as Assets.Scripts.monitor_import)

EXPORT_NAME = "export.csv"
SHOTS_NAME = "shots.csv"
SHOTS_FIELDS = ["shot_name", "reported_club", "speed_mph", "vla_deg", "hla_deg", "total_spin_rpm", "spin_axis_deg", "backspin_rpm",
                "sidespin_rpm", "carry_yd", "total_yd", "rollout_yd", "apex_ft", "landing_angle_deg", "offline_yd"]
# Export column -> (shots.csv column, signed). Signed columns carry an L or R prefix.
COLUMNS = {
    "Ball Speed(mph)": ("speed_mph", False), "Launch Direction": ("hla_deg", True), "Launch Angle": ("vla_deg", False),
    "Spin Rate": ("total_spin_rpm", False), "Spin Axis": ("spin_axis_deg", True), "Back Spin": ("backspin_rpm", False),
    "Side Spin": ("sidespin_rpm", True), "Apex(yd)": ("apex_ft", False), "Carry(yd)": ("carry_yd", False),
    "Total(yd)": ("total_yd", False), "Offline(yd)": ("offline_yd", True), "Landing Angle": ("landing_angle_deg", False),
}
DATE_PATTERN = re.compile(r"(\d{1,2}) ([A-Za-z]{3})\.? (\d{4})")


def signed(text):
    """'L7.1' -> -7.1, 'R8.4' -> 8.4, '0.0' -> 0.0."""
    text = text.strip()
    if text[:1] in ("L", "R"):
        value = float(text[1:])
        return -value if text[0] == "L" else value
    return float(text)


def club_slug(club):
    """'3 Hybrid' -> '3hybrid'."""
    return re.sub(r"[^a-z0-9]", "", club.lower())


def session_date(lines):
    """yyyymmdd from a 'Dates,21:32 13 Nov. 2026,...' line, or None."""
    for line in lines:
        if line.startswith("Dates"):
            match = DATE_PATTERN.search(line)
            if match:
                return datetime.strptime(" ".join(match.groups()), "%d %b %Y").strftime("%Y%m%d")
    return None


def parse_export(text):
    """Shot rows of an export as shots.csv dictionaries, in export order."""
    lines = text.splitlines()
    header_index = next((index for index, line in enumerate(lines) if line.startswith("Club,Index,")), None)
    if header_index is None:
        raise ValueError("no 'Club,Index,...' header line")
    reader = csv.DictReader(lines[header_index:])
    missing = [column for column in COLUMNS if column not in reader.fieldnames]
    if missing:
        raise ValueError("missing columns: " + ", ".join(missing))
    shots, names = [], set()
    for row in reader:
        club, index = (row.get("Club") or "").strip(), (row.get("Index") or "").strip()
        # Average and Deviation rows have no club and a word for an index.
        if not club or not index.isdigit():
            continue
        shot = {"shot_name": f"{club_slug(club)}_{int(index):02d}", "reported_club": club}
        for column, (field, is_signed) in COLUMNS.items():
            value = signed(row[column]) if is_signed else float(row[column])
            shot[field] = value
        shot["rollout_yd"] = shot["total_yd"] - shot["carry_yd"]
        if shot["shot_name"] in names:
            raise ValueError(f"duplicate shot {shot['shot_name']}")
        names.add(shot["shot_name"])
        shots.append(shot)
    return shots


SPIN_FIELDS = ("total_spin_rpm", "backspin_rpm", "sidespin_rpm")


def format_value(field, value):
    """Text as written: names unchanged, spin in whole rpm, everything else to hundredths."""
    if isinstance(value, str):
        return value
    return f"{value:.0f}" if field in SPIN_FIELDS else f"{value:.2f}"


def write_shots(shots, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=SHOTS_FIELDS, lineterminator="\n")
        writer.writeheader()
        for shot in shots:
            writer.writerow({field: format_value(field, shot[field]) for field in SHOTS_FIELDS})


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("export", type=Path, help="the launch-monitor app's CSV export")
    parser.add_argument("--session", help="session folder name (default export_<yyyymmdd> from the export)")
    parser.add_argument("--library", type=Path, default=None, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    text = args.export.read_text(encoding="utf-8-sig")
    shots = parse_export(text)
    date = session_date(text.splitlines())
    session = args.session or (f"export_{date}" if date else None)
    if not session:
        parser.error("the export has no date line; pass --session")
    folder = (args.library or local_library_dir()) / session
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.export, folder / EXPORT_NAME)
    write_shots(shots, folder / SHOTS_NAME)
    speeds = [shot["speed_mph"] for shot in shots]
    print(f"Imported {len(shots)} shots ({min(speeds):.1f}-{max(speeds):.1f} mph ball speed) into {folder}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
