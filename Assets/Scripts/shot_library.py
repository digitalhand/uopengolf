# SPDX-License-Identifier: MIT
"""The shot library and the engine's replay results, shared by the shot-library scripts.

A session is a folder holding a shots.csv (format: Assets/Data/README.md), named after its path relative to its
library: the repository's recordings (Assets/Data/ShotLibrary) and, when present, the local reference data
(data_paths.py). Engine results come from the UOpenGolf.Physics.ShotLibrary automation test
(Saved/ShotLibrary/results.csv). Nothing here reads the reported club.
"""
import csv
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from data_paths import ROOT, shot_library_dirs  # noqa: E402  (run as a script or imported as Assets.Scripts.shot_library)

SHOTS_NAME = "shots.csv"
RESULTS_PATH = ROOT / "Saved" / "ShotLibrary" / "results.csv"

# The engine's unit factors (Plugins/OpenFairwayPhysics/Source/OpenFairwayPhysics/Public/OpenFairway/Units.h).
MPS_PER_MPH = 0.44704
RAD_PER_RPM = 0.10472
PA_PER_PSI = 6894.757
# Exact conversions for reports.
YARDS_PER_METER = 1.0936133
FEET_PER_METER = 3.2808399

# Reference columns of shots.csv for each metric other than roll.
REFERENCE_COLUMNS = (("carry", "carry_yd"), ("total", "total_yd"), ("apex", "apex_ft"), ("landing", "landing_angle_deg"))


def optional_float(row, column, default=None):
    """The column as a float, or default when it is missing or empty. Zero is a value (a ground shot carries 0)."""
    text = (row.get(column) or "").strip()
    return float(text) if text else default


def as_dirs(library_dirs):
    """One library folder or several."""
    return [Path(library_dirs)] if isinstance(library_dirs, (str, Path)) else [Path(d) for d in library_dirs]


def load_library(library_dirs=None):
    """Every named row of every <session>/shots.csv in each library folder, in file order.

    Each shot has its session and name, its launch (speed mph, vla and hla deg, back, side and total spin rpm),
    the air its reference was computed in (temperature_f, humidity 0-1, pressure_psi; None where not given)
    and its reference results (carry, total, roll, apex, landing; None where not given).
    """
    shots = []
    for library_dir in as_dirs(shot_library_dirs() if library_dirs is None else library_dirs):
        shots += _load_library_dir(library_dir.resolve())
    return shots


def _load_library_dir(library_dir):
    shots = []
    for path in sorted(library_dir.rglob(SHOTS_NAME)):
        session = path.parent.relative_to(library_dir).as_posix()
        with path.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                name = (row.get("shot_name") or "").strip()
                if not name:
                    continue
                ref = {key: optional_float(row, column) for key, column in REFERENCE_COLUMNS}
                ref["roll"] = ref["total"] - ref["carry"] if ref["carry"] is not None and ref["total"] is not None else None
                shots.append({
                    "session": session, "shot": name,
                    "speed": float(row["speed_mph"]), "vla": float(row["vla_deg"]), "hla": optional_float(row, "hla_deg", 0.0),
                    "back": optional_float(row, "backspin_rpm", 0.0), "side": optional_float(row, "sidespin_rpm", 0.0),
                    "spin": float(row["total_spin_rpm"]),
                    "air": {"temperature_f": optional_float(row, "temperature_f"), "humidity": optional_float(row, "humidity"),
                            "pressure_psi": optional_float(row, "pressure_psi")},
                    "ref": ref,
                })
    return shots


def load_results(path=RESULTS_PATH):
    """Engine results keyed by (session, shot_name, profile)."""
    results = {}
    with Path(path).open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            key = (row["session"], row["shot_name"], row["profile"])
            if key in results:
                raise ValueError(f"Duplicate result: {key}")
            results[key] = {
                "carry": float(row["carry_yd"]), "total": float(row["total_yd"]),
                "roll": float(row["rollout_yd"]), "apex": float(row["apex_ft"]),
                "landing": optional_float(row, "landing_angle_deg"),
                "landing_spin": optional_float(row, "landing_spin_rpm"),
                "at_rest": row.get("at_rest") == "true",
                "hang": optional_float(row, "hang_s"),
                "lateral": optional_float(row, "lateral_yd"),
            }
    return results
