# SPDX-License-Identifier: MIT
"""Show how far the uopengolf physics are from FlightScope for every shot in the library.

Unless --skip-run is given, this runs the UOpenGolf.Physics.ShotLibrary automation test,
which replays each shot in Data/ShotLibrary/*/fs.csv through the headless simulator and
writes Saved/ShotLibrary/results.csv. It then prints engine minus FlightScope per shot
(+ means the engine goes longer or higher), a summary for the current and historical
profiles, and the change since the last run whose results differed.
"""
import argparse
import csv
from datetime import datetime
import io
from pathlib import Path
import platform
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
LIBRARY_DIR = ROOT / "Data" / "ShotLibrary"
RESULTS_PATH = ROOT / "Saved" / "ShotLibrary" / "results.csv"
HISTORY_DIR = ROOT / "Saved" / "ShotLibrary" / "history"
TEST_NAME = "UOpenGolf.Physics.ShotLibrary"
# Listed first in reports; any other profile in the results follows in first-seen order.
LEADING_PROFILES = ("current", "historical")
# (key, label, unit); roll is total minus carry for both the engine and FlightScope.
METRICS = (("carry", "Carry", "yd"), ("total", "Total", "yd"), ("roll", "Roll", "yd"), ("apex", "Apex", "ft"))
# Report bins for the rollout breakdown, a simplification of LandingBucket::Classify.
SPIN_BINS = ((4000.0, "Run"), (5500.0, "Release"), (7500.0, "Check"), (float("inf"), "Spinback"))
LANDING_BANDS = ((35.0, "<35"), (45.0, "35-45"), (float("inf"), ">=45"))
# The grades used by openfairway's compare_csv.py.
CARRY_PASS, CARRY_MODERATE = 3.0, 7.0
TOTAL_PASS, TOTAL_MODERATE = 5.0, 10.0
WITHIN = (3.0, 5.0, 10.0)
SNAPSHOT_FIELDS = ["session", "shot_name", "club", "profile", "status"] + [
    f"{prefix}{metric}" for metric, _, _ in METRICS for prefix in ("", "fs_", "diff_")]
# History snapshots are named <local time>.csv, with -2, -3... for runs in the same second.
SNAPSHOT_NAME = re.compile(r"(\d{8}-\d{6})(?:-(\d+))?")


def club_of(shot_name):
    """Club from recorded names such as shot_7i_26 or shot_driver_42; 'other' when unnamed."""
    match = re.fullmatch(r"shot_([a-z0-9]+)_\d+", shot_name)
    return match.group(1) if match else "other"


def reference_value(row, column):
    """FlightScope value, or None when missing; the library uses 0 for 'no result'."""
    text = (row.get(column) or "").strip()
    if not text:
        return None
    value = float(text)
    return None if value == 0.0 else value


def load_library(library_dir):
    """Every row of every <session>/fs.csv, in file order."""
    library_dir = Path(library_dir).resolve()
    shots = []
    for path in sorted(library_dir.rglob("fs.csv")):
        session = path.parent.relative_to(library_dir).as_posix()
        with path.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                name = (row.get("shot_name") or "").strip()
                if not name:
                    continue
                fs = {key: reference_value(row, column) for key, column in
                      (("carry", "carry_yd"), ("total", "total_yd"), ("apex", "apex_ft"))}
                fs["roll"] = fs["total"] - fs["carry"] if fs["carry"] is not None and fs["total"] is not None else None
                shots.append({
                    "session": session, "shot": name, "club": club_of(name),
                    "speed": float(row["speed_mph"]), "vla": float(row["vla_deg"]),
                    "spin": float(row["total_spin_rpm"]), "fs": fs,
                })
    return shots


def optional_float(row, column):
    """A float, or None when the column is missing or empty (results from older builds)."""
    text = (row.get(column) or "").strip()
    return float(text) if text else None


def load_results(path):
    """Engine results keyed by (session, shot_name, profile)."""
    results = {}
    with Path(path).open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            results[(row["session"], row["shot_name"], row["profile"])] = {
                "carry": float(row["carry_yd"]), "total": float(row["total_yd"]),
                "roll": float(row["rollout_yd"]), "apex": float(row["apex_ft"]),
                "landing_angle": optional_float(row, "landing_angle_deg"),
                "landing_spin": optional_float(row, "landing_spin_rpm"),
            }
    return results


def profiles_in(results):
    """Profiles present in the results: current and historical first, then first-seen order."""
    seen = list(dict.fromkeys(profile for _, _, profile in results))
    return [profile for profile in LEADING_PROFILES if profile in seen] + [
        profile for profile in seen if profile not in LEADING_PROFILES]


def grade(carry_diff, total_diff):
    """pass, moderate or severe, from the worse of the carry and total differences."""
    carry = abs(carry_diff) if carry_diff is not None else 0.0
    total = abs(total_diff) if total_diff is not None else 0.0
    if total > TOTAL_MODERATE or carry > CARRY_MODERATE:
        return "severe"
    if total > TOTAL_PASS or carry > CARRY_PASS:
        return "moderate"
    return "pass"


def compare(shots, results, profile):
    """Rows for shots with a FlightScope carry; returns (rows, shots without an engine result)."""
    rows, missing = [], []
    for shot in shots:
        if shot["fs"]["carry"] is None:
            continue
        engine = results.get((shot["session"], shot["shot"], profile))
        if engine is None:
            missing.append(shot)
            continue
        row = {key: shot[key] for key in ("session", "shot", "club", "speed", "vla", "spin")}
        row["profile"] = profile
        row["landing_angle"] = engine["landing_angle"]
        row["landing_spin"] = engine["landing_spin"]
        for metric, _, _ in METRICS:
            reference = shot["fs"][metric]
            row[metric] = engine[metric]
            row["fs_" + metric] = reference
            row["diff_" + metric] = None if reference is None else engine[metric] - reference
        row["status"] = grade(row["diff_carry"], row["diff_total"])
        rows.append(row)
    return rows, missing


def summarize(rows, metric):
    """Bias (mean +/-), average |+/-|, worst +/- and the share within each WITHIN threshold."""
    diffs = [row["diff_" + metric] for row in rows if row["diff_" + metric] is not None]
    if not diffs:
        return None
    return {
        "count": len(diffs),
        "bias": sum(diffs) / len(diffs),
        "avg": sum(abs(diff) for diff in diffs) / len(diffs),
        "worst": max(diffs, key=abs),
        "within": [100.0 * sum(abs(diff) <= limit for diff in diffs) / len(diffs) for limit in WITHIN],
    }


def fmt(value, spec):
    return "-" if value is None else format(value, spec)


def render(header, rows, right, breaks=()):
    """Left/right-aligned columns; '|' before each column index in breaks."""
    widths = [max(len(cell) for cell in column) for column in zip(header, *rows)]
    lines = []
    for cells in [header, ["-" * width for width in widths]] + rows:
        parts = []
        for index, (cell, width) in enumerate(zip(cells, widths)):
            if index in breaks:
                parts.append("|")
            parts.append(cell.rjust(width) if index in right else cell.ljust(width))
        lines.append(" ".join(parts).rstrip())
    return "\n".join(lines)


def shot_table(rows):
    header = ["Session", "Shot", "Club", "Speed", "VLA", "Spin"]
    for _, label, _ in METRICS:
        header += [label, "FS", "+/-"]
    header.append("Status")
    body = []
    for row in rows:
        cells = [row["session"], row["shot"], row["club"], fmt(row["speed"], ".1f"), fmt(row["vla"], ".1f"), fmt(row["spin"], ".0f")]
        for metric, _, _ in METRICS:
            cells += [fmt(row[metric], ".1f"), fmt(row["fs_" + metric], ".1f"), fmt(row["diff_" + metric], "+.1f")]
        cells.append(row["status"])
        body.append(cells)
    right = set(range(3, len(header) - 1))
    return render(header, body, right, breaks={6, 9, 12, 15, 18})


def summary_table(rows_by_profile):
    header = ["Profile", "Metric", "Bias", "Avg |+/-|", "Worst"] + [f"<={limit:g}" for limit in WITHIN]
    body = []
    for profile in rows_by_profile:
        for index, (metric, label, unit) in enumerate(METRICS):
            cells = [profile if index == 0 else "", f"{label} ({unit})"]
            stats = summarize(rows_by_profile[profile], metric)
            if stats is None:
                cells += ["-"] * (3 + len(WITHIN))
            else:
                cells += [format(stats["bias"], "+.1f"), format(stats["avg"], ".1f"), format(stats["worst"], "+.1f")]
                cells += [f"{share:.0f}%" for share in stats["within"]]
            body.append(cells)
    lines = [render(header, body, set(range(2, len(header))))]
    for profile in rows_by_profile:
        counts = [sum(row["status"] == status for row in rows_by_profile[profile]) for status in ("pass", "moderate", "severe")]
        lines.append(f"Status ({profile}): pass {counts[0]}, moderate {counts[1]}, severe {counts[2]}")
    return "\n".join(lines)


def bin_label(value, bins):
    return next(label for limit, label in bins if value < limit)


def rollout_cells(rows):
    """Engine and FlightScope roll per (spin bin, landing band) over rows with both values."""
    groups = {}
    for row in rows:
        if row["landing_angle"] is None or row["fs_roll"] is None:
            continue
        key = (bin_label(row["spin"], SPIN_BINS), bin_label(row["landing_angle"], LANDING_BANDS))
        groups.setdefault(key, []).append(row)
    return {key: {"count": len(group),
                  "engine": sum(row["roll"] for row in group) / len(group),
                  "fs": sum(row["fs_roll"] for row in group) / len(group),
                  "bias": sum(row["diff_roll"] for row in group) / len(group)}
            for key, group in groups.items()}


def rollout_table(rows):
    cells = rollout_cells(rows)
    order = {label: index for index, (_, label) in enumerate(SPIN_BINS + LANDING_BANDS)}
    header = ["Spin", "Landing (deg)", "Shots", "Roll", "FS roll", "Bias"]
    body = [[spin, band, str(cell["count"]), format(cell["engine"], ".1f"), format(cell["fs"], ".1f"), format(cell["bias"], "+.1f")]
            for (spin, band), cell in sorted(cells.items(), key=lambda item: (order[item[0][0]], order[item[0][1]]))]
    return render(header, body, set(range(2, len(header))))


def club_table(rows):
    clubs = sorted({row["club"] for row in rows}, key=lambda club: (club == "other", club))
    header = ["Club", "Shots", "Carry bias", "Carry avg|+/-|", "Total bias", "Total avg|+/-|", "Apex bias", "Apex avg|+/-|"]
    body = []
    for club in clubs:
        group = [row for row in rows if row["club"] == club]
        cells = [club, str(len(group))]
        for metric in ("carry", "total", "apex"):
            stats = summarize(group, metric)
            cells += ["-", "-"] if stats is None else [format(stats["bias"], "+.1f"), format(stats["avg"], ".1f")]
        body.append(cells)
    return render(header, body, set(range(1, len(header))))


def snapshot_text(rows):
    """Every profile's rows as CSV; identical text means identical engine results."""
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=SNAPSHOT_FIELDS, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        record = {"session": row["session"], "shot_name": row["shot"], "club": row["club"],
                  "profile": row["profile"], "status": row["status"]}
        for metric, _, _ in METRICS:
            for prefix in ("", "fs_", "diff_"):
                value = row[prefix + metric]
                record[prefix + metric] = "" if value is None else f"{value:.4f}"
        writer.writerow(record)
    return output.getvalue()


def snapshot_order(path):
    """Run order from a snapshot name: its timestamp, then the collision suffix (none is 1)."""
    match = SNAPSHOT_NAME.fullmatch(path.stem)
    return match.group(1), int(match.group(2) or 1)


def record_history(history_dir, snapshot, now):
    """Saves the snapshot unless the newest one matches.

    Returns (snapshot path, whether it is new, newest earlier snapshot with different results).
    """
    history_dir = Path(history_dir)
    history_dir.mkdir(parents=True, exist_ok=True)
    earlier = sorted((path for path in history_dir.glob("*.csv") if SNAPSHOT_NAME.fullmatch(path.stem)), key=snapshot_order)
    contents = {path: path.read_text(encoding="utf-8") for path in earlier}
    baseline = next((path for path in reversed(earlier) if contents[path] != snapshot), None)
    if earlier and contents[earlier[-1]] == snapshot:
        return earlier[-1], False, baseline
    path = history_dir / f"{now:%Y%m%d-%H%M%S}.csv"
    suffix = 2
    while path.exists():
        path = history_dir / f"{now:%Y%m%d-%H%M%S}-{suffix}.csv"
        suffix += 1
    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(snapshot)
    return path, True, baseline


def load_snapshot(path, profile):
    """Rows of one profile from a history snapshot, in the shape compare() returns."""
    rows = []
    with Path(path).open(newline="", encoding="utf-8") as handle:
        for record in csv.DictReader(handle):
            if record["profile"] != profile:
                continue
            row = {"session": record["session"], "shot": record["shot_name"], "club": record["club"]}
            for metric, _, _ in METRICS:
                for prefix in ("", "fs_", "diff_"):
                    value = record.get(prefix + metric, "")
                    row[prefix + metric] = float(value) if value else None
            rows.append(row)
    return rows


def change_report(rows, baseline_rows, baseline_name):
    """Current-profile change over the shots present in both runs."""
    previous = {(row["session"], row["shot"]): row for row in baseline_rows}
    now = [row for row in rows if (row["session"], row["shot"]) in previous]
    before = [previous[(row["session"], row["shot"])] for row in now]
    lines = [f"Change since run {baseline_name} (current profile, {len(now)} shots in both runs):"]
    for metric, label, unit in METRICS:
        old, new = summarize(before, metric), summarize(now, metric)
        if old is None or new is None:
            continue
        change = round(new["avg"] - old["avg"], 2) + 0.0  # + 0.0 turns -0.0 into 0.0
        lines.append(f"  {label} ({unit}): avg|+/-| {old['avg']:.2f} -> {new['avg']:.2f} ({change:+.2f}),"
                     f" bias {old['bias']:+.2f} -> {new['bias']:+.2f}")
    return "\n".join(lines)


def run_engine(build):
    """Builds (optionally) and runs the replay test; returns the test's exit code."""
    if platform.system() == "Windows":
        base = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "Scripts" / "ue.ps1")]
        build_command, test_command = base + ["Build"], base + ["Test", "-Filter", TEST_NAME]
    else:
        base = ["bash", str(ROOT / "Scripts" / "ue.sh")]
        build_command, test_command = base + ["build"], base + ["test", "--filter", TEST_NAME]
    if build and subprocess.call(build_command, cwd=ROOT) != 0:
        sys.exit("Editor build failed; see Saved/Logs/.")
    if RESULTS_PATH.exists():
        RESULTS_PATH.unlink()
    code = subprocess.call(test_command, cwd=ROOT)
    if not RESULTS_PATH.exists():
        sys.exit(f"{TEST_NAME} did not write {RESULTS_PATH}; see the newest report under Saved/Automation/.")
    return code


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--skip-run", action="store_true", help="report on the last results without running Unreal")
    parser.add_argument("--build", action="store_true", help="build the editor before running (after C++ changes)")
    parser.add_argument("--session", action="append", help="only this session, such as session_5 (repeatable)")
    parser.add_argument("--club", action="append", help="only this club, such as driver or 7i (repeatable)")
    parser.add_argument("--sort", choices=[metric for metric, _, _ in METRICS], help="largest |+/-| of this metric first")
    parser.add_argument("--profile", default="current", help="profile for the per-shot table (current, historical or a candidate)")
    parser.add_argument("--baseline", type=Path, help="history snapshot to compare with instead of the last different run")
    parser.add_argument("--library", type=Path, default=LIBRARY_DIR, help=argparse.SUPPRESS)
    parser.add_argument("--results", type=Path, default=RESULTS_PATH, help=argparse.SUPPRESS)
    parser.add_argument("--history", type=Path, default=HISTORY_DIR, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if args.skip_run and args.build:
        parser.error("--build needs a run; drop --skip-run")
    if not args.skip_run and args.results.resolve() != RESULTS_PATH.resolve():
        parser.error("--results is only for --skip-run")
    return args


def main(argv=None):
    args = parse_args(argv)
    exit_code = 0
    if not args.skip_run and run_engine(args.build) != 0:
        print("WARNING: the automation run failed; the results below may be incomplete.", file=sys.stderr)
        exit_code = 1
    if not args.results.exists():
        sys.exit(f"No results at {args.results}; run without --skip-run first.")
    shots = load_library(args.library)
    results = load_results(args.results)
    profiles = profiles_in(results)
    if "current" not in profiles:
        sys.exit(f"No current profile in {args.results}; it has: {', '.join(profiles)}.")
    if args.profile not in profiles:
        sys.exit(f"No {args.profile} profile in {args.results}; it has: {', '.join(profiles)}.")
    compared = {profile: compare(shots, results, profile) for profile in profiles}

    def selected(rows):
        return [row for row in rows if (not args.session or row["session"] in args.session)
                and (not args.club or row["club"] in args.club)]

    rows_by_profile = {profile: selected(compared[profile][0]) for profile in profiles}
    table_rows = list(rows_by_profile[args.profile])
    if args.sort:
        key = "diff_" + args.sort
        table_rows.sort(key=lambda row: -1.0 if row[key] is None else abs(row[key]), reverse=True)

    skipped = sum(shot["fs"]["carry"] is None for shot in selected(shots))
    print(f"Engine ({args.profile} profile) vs FlightScope: engine value, FlightScope value and +/- (engine minus"
          f" FlightScope; + is longer or higher). Yards; apex in feet.")
    print(shot_table(table_rows))
    print()
    print(f"Summary over {len(rows_by_profile['current'])} shots ({skipped} library shots have no FlightScope result and are"
          f" not compared). Within columns count |+/-| in yd, or ft for apex.")
    print(summary_table(rows_by_profile))
    print()
    print("By club (current profile):")
    print(club_table(rows_by_profile["current"]))
    if any(row["landing_angle"] is not None for row in rows_by_profile["current"]):
        print()
        print("Rollout by spin and landing angle (current profile):")
        print(rollout_table(rows_by_profile["current"]))
    for profile in profiles:
        missing = compared[profile][1]
        if missing:
            names = ", ".join(f"{shot['session']}/{shot['shot']}" for shot in missing)
            print(f"WARNING: no {profile} engine result for {len(missing)} shot(s): {names}", file=sys.stderr)
            exit_code = 1

    snapshot = snapshot_text([row for profile in profiles for row in compared[profile][0]])
    saved, is_new, baseline = record_history(args.history, snapshot, datetime.now())
    if args.baseline:
        baseline = args.baseline
    print()
    if baseline:
        print(change_report(rows_by_profile["current"], selected(load_snapshot(baseline, "current")), baseline.stem))
    else:
        print("No earlier run with different results to compare with yet.")
    print(f"Engine results: {args.results}")
    print(f"Snapshot: {saved}" + ("" if is_new else " (results unchanged since this run)"))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
