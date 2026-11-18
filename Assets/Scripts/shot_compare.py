# SPDX-License-Identifier: MIT
"""Show how far the uopengolf physics are from the launch monitor for every shot in the library.

Unless --skip-run is given, this runs the UOpenGolf.Physics.ShotLibrary automation test, which
replays each shot in Assets/Data/ShotLibrary/*/shots.csv through the headless simulator (flat
fairway, 75 F, sea level) and writes Saved/ShotLibrary/results.csv. It then prints engine minus
reference per shot (+ means the engine goes longer, higher or steeper), summaries for every
profile, roll by launch class, errors by ball speed, and the change since the last run whose
results differed. Shots are grouped only by their launch values: the club a launch monitor or the
game reports (the reported_club column) is not reliable, and nothing here reads it.
"""
import argparse
import csv
from datetime import datetime
import io
import hashlib
import json
import math
from pathlib import Path
import platform
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPTS_DIR = ROOT / "Assets" / "Scripts"
LIBRARY_DIR = ROOT / "Assets" / "Data" / "ShotLibrary"
SHOTS_NAME = "shots.csv"
RESULTS_PATH = ROOT / "Saved" / "ShotLibrary" / "results.csv"
HISTORY_DIR = ROOT / "Saved" / "ShotLibrary" / "history"
TEST_NAME = "UOpenGolf.Physics.ShotLibrary"
# Listed first in reports; any other profile in the results follows in first-seen order.
LEADING_PROFILES = ("current", "historical")
# (key, label, unit); roll is total minus carry for both the engine and the reference.
METRICS = (("carry", "Carry", "yd"), ("total", "Total", "yd"), ("roll", "Roll", "yd"), ("apex", "Apex", "ft"),
           ("landing", "Landing", "deg"))
# Reference columns of shots.csv for each metric other than roll.
REFERENCE_COLUMNS = (("carry", "carry_yd"), ("total", "total_yd"), ("apex", "apex_ft"), ("landing", "landing_angle_deg"))
# Launch classes for the roll breakdown: (upper vertical launch, label); mid launches split by spin.
LAUNCH_CLASSES = ((2.5, "ground (<2.5 deg)"), (9.0, "low (2.5-9 deg)"), (20.0, "mid (9-20 deg)"), (float("inf"), "high (>=20 deg)"))
MID_SPLIT_RPM = 4000.0
# Ball speed bands for the error breakdown: (upper speed in mph, label).
SPEED_BANDS = ((60.0, "<60 mph"), (75.0, "60-75 mph"), (90.0, "75-90 mph"), (105.0, "90-105 mph"), (float("inf"), ">=105 mph"))
# Report bins for the rollout breakdown, a simplification of LandingBucket::Classify.
SPIN_BINS = ((4000.0, "Run"), (5500.0, "Release"), (7500.0, "Check"), (float("inf"), "Spinback"))
LANDING_BANDS = ((35.0, "<35"), (45.0, "35-45"), (float("inf"), ">=45"))
# Grades by the worse of carry and total.
CARRY_PASS, CARRY_MODERATE = 3.0, 7.0
TOTAL_PASS, TOTAL_MODERATE = 5.0, 10.0
WITHIN = (3.0, 5.0, 10.0)
SNAPSHOT_FIELDS = ["session", "shot_name", "profile", "status"] + [
    f"{prefix}{metric}" for metric, _, _ in METRICS for prefix in ("", "ref_", "diff_")]
# History snapshots are named <local time>.csv, with -2, -3... for runs in the same second.
SNAPSHOT_NAME = re.compile(r"(\d{8}-\d{6})(?:-(\d+))?")


def reference_value(row, column):
    """Reference value, or None when the column is missing or empty. Zero is a value (a ground shot carries 0)."""
    text = (row.get(column) or "").strip()
    return float(text) if text else None


def launch_class(vla, spin):
    label = next(label for limit, label in LAUNCH_CLASSES if vla < limit)
    if label.startswith("mid"):
        return f"mid (9-20 deg, {'<' if spin < MID_SPLIT_RPM else '>='}{MID_SPLIT_RPM:.0f} rpm)"
    return label


def load_library(library_dir):
    """Every row of every <session>/shots.csv, in file order."""
    library_dir = Path(library_dir).resolve()
    shots = []
    for path in sorted(library_dir.rglob(SHOTS_NAME)):
        session = path.parent.relative_to(library_dir).as_posix()
        with path.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                name = (row.get("shot_name") or "").strip()
                if not name:
                    continue
                ref = {key: reference_value(row, column) for key, column in REFERENCE_COLUMNS}
                ref["roll"] = ref["total"] - ref["carry"] if ref["carry"] is not None and ref["total"] is not None else None
                shots.append({
                    "session": session, "shot": name,
                    "speed": float(row["speed_mph"]), "vla": float(row["vla_deg"]),
                    "spin": float(row["total_spin_rpm"]), "ref": ref,
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
    """Rows for shots with a reference total; returns (rows, shots without an engine result)."""
    rows, missing = [], []
    for shot in shots:
        if shot["ref"]["total"] is None:
            continue
        engine = results.get((shot["session"], shot["shot"], profile))
        if engine is None:
            missing.append(shot)
            continue
        row = {key: shot[key] for key in ("session", "shot", "speed", "vla", "spin")}
        row["profile"] = profile
        row["class"] = launch_class(shot["vla"], shot["spin"])
        row["landing_spin"] = engine["landing_spin"]
        for metric, _, _ in METRICS:
            reference = shot["ref"][metric]
            row[metric] = engine[metric]
            row["ref_" + metric] = reference
            row["diff_" + metric] = None if reference is None or engine[metric] is None else engine[metric] - reference
        row["status"] = grade(row["diff_carry"], row["diff_total"])
        rows.append(row)
    return rows, missing


def summarize(rows, metric):
    """Bias (mean +/-), average |+/-|, worst +/- and the share within each WITHIN threshold."""
    diffs = [row["diff_" + metric] for row in rows if row.get("diff_" + metric) is not None]
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
    header = ["Session", "Shot", "Speed", "VLA", "Spin"]
    for _, label, _ in METRICS:
        header += [label, "Ref", "+/-"]
    header.append("Status")
    body = []
    for row in rows:
        cells = [row["session"], row["shot"], fmt(row["speed"], ".1f"), fmt(row["vla"], ".1f"), fmt(row["spin"], ".0f")]
        for metric, _, _ in METRICS:
            cells += [fmt(row[metric], ".1f"), fmt(row["ref_" + metric], ".1f"), fmt(row["diff_" + metric], "+.1f")]
        cells.append(row["status"])
        body.append(cells)
    right = set(range(2, len(header) - 1))
    return render(header, body, right, breaks={5 + 3 * index for index in range(len(METRICS))})


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


def class_order(label):
    """Ground to high, and below the mid spin split before above it."""
    return [class_label.split()[0] for _, class_label in LAUNCH_CLASSES].index(label.split()[0]), label


def class_table(rows_by_profile):
    """Roll bias and average |+/-| per launch class, one column pair per profile."""
    classes = sorted({row["class"] for rows in rows_by_profile.values() for row in rows}, key=class_order)
    header = ["Launch class", "Shots", "mph range", "rpm range", "Ref roll"]
    for profile in rows_by_profile:
        header += [f"{profile} roll", "bias", "avg|+/-|"]
    body = []
    for label in classes + ["all"]:
        def group(profile):
            return [row for row in rows_by_profile[profile] if label == "all" or row["class"] == label]
        first = group(next(iter(rows_by_profile)))
        references = [row["ref_roll"] for row in first if row["ref_roll"] is not None]
        speed_range = f"{min(r['speed'] for r in first):.1f}-{max(r['speed'] for r in first):.1f}" if first else "-"
        spin_range = f"{min(r['spin'] for r in first):.0f}-{max(r['spin'] for r in first):.0f}" if first else "-"
        cells = [label, str(len(first)), speed_range, spin_range, fmt(sum(references) / len(references) if references else None, ".1f")]
        for profile in rows_by_profile:
            rows = group(profile)
            stats = summarize(rows, "roll")
            cells += [fmt(sum(row["roll"] for row in rows) / len(rows) if rows else None, ".1f")]
            cells += ["-", "-"] if stats is None else [format(stats["bias"], "+.1f"), format(stats["avg"], ".1f")]
        body.append(cells)
    return render(header, body, set(range(1, len(header))))


def bin_label(value, bins):
    return next(label for limit, label in bins if value < limit)


def rollout_cells(rows):
    """Engine and reference roll per (spin bin, landing band) over rows with both values."""
    groups = {}
    for row in rows:
        if row["landing"] is None or row["ref_roll"] is None:
            continue
        key = (bin_label(row["spin"], SPIN_BINS), bin_label(row["landing"], LANDING_BANDS))
        groups.setdefault(key, []).append(row)
    return {key: {"count": len(group),
                  "engine": sum(row["roll"] for row in group) / len(group),
                  "ref": sum(row["ref_roll"] for row in group) / len(group),
                  "bias": sum(row["diff_roll"] for row in group) / len(group)}
            for key, group in groups.items()}


def rollout_table(rows):
    cells = rollout_cells(rows)
    order = {label: index for index, (_, label) in enumerate(SPIN_BINS + LANDING_BANDS)}
    header = ["Spin", "Landing (deg)", "Shots", "Roll", "Ref roll", "Bias"]
    body = [[spin, band, str(cell["count"]), format(cell["engine"], ".1f"), format(cell["ref"], ".1f"), format(cell["bias"], "+.1f")]
            for (spin, band), cell in sorted(cells.items(), key=lambda item: (order[item[0][0]], order[item[0][1]]))]
    return render(header, body, set(range(2, len(header))))


def speed_table(rows):
    """Carry, total and roll bias and average |+/-| per ball speed band."""
    header = ["Ball speed", "Shots", "Carry bias", "Carry avg|+/-|", "Total bias", "Total avg|+/-|", "Roll bias", "Roll avg|+/-|"]
    body = []
    for _, label in SPEED_BANDS:
        group = [row for row in rows if bin_label(row["speed"], SPEED_BANDS) == label]
        if not group:
            continue
        cells = [label, str(len(group))]
        for metric in ("carry", "total", "roll"):
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
        record = {"session": row["session"], "shot_name": row["shot"], "profile": row["profile"], "status": row["status"]}
        for metric, _, _ in METRICS:
            for prefix in ("", "ref_", "diff_"):
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
            row = {"session": record["session"], "shot": record["shot_name"]}
            for metric, _, _ in METRICS:
                for prefix in ("", "ref_", "diff_"):
                    value = record.get(prefix + metric) or ""
                    row[prefix + metric] = float(value) if value else None
            rows.append(row)
    return rows


def change_report(rows, baseline_rows, baseline_name):
    """Current-profile change over the shots present in both runs."""
    previous = {(row["session"], row["shot"]): row for row in baseline_rows}
    now = [row for row in rows if (row["session"], row["shot"]) in previous]
    if not now:
        return f"Run {baseline_name} has none of these shots; nothing to compare with yet."
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


def fingerprint(library_dir=LIBRARY_DIR):
    """Hash exact source, replay inputs, and binaries, including uncommitted work."""
    roots = [ROOT / "Plugins/OpenFairwayPhysics/Source", ROOT / "Source", Path(library_dir)]
    files = [p for root in roots for p in root.rglob("*") if p.is_file() and p.suffix in (".cpp", ".h", ".csv")]
    files += list((ROOT / "Plugins/OpenFairwayPhysics/Binaries").rglob("*OpenFairwayPhysics*.dll"))
    files += list((ROOT / "Binaries").rglob("*UOpenGolf*.dll"))
    digest = hashlib.sha256()
    for path in sorted(files):
        digest.update(str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path).encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def validate_accuracy(shots, results, baseline_results):
    """Acceptance is based on launch values; no gate reads a club."""
    errors = []
    for shot in shots:
        key = (shot["session"], shot["shot"], "current")
        result = results.get(key)
        if result is None:
            errors.append(f"Missing result: {key}")
        elif not result.get("at_rest") or any(not math.isfinite(value) for value in result.values() if isinstance(value, (int, float))):
            errors.append(f"Nonfinite or non-resting result: {key}")
    rows, missing = compare(shots, results, "current")
    before, baseline_missing = compare(shots, baseline_results, "current")
    if missing or baseline_missing or not rows:
        return errors + ["Accuracy comparison needs complete current and baseline reference coverage"]
    if errors:
        return errors
    for metric in ("roll", "total"):
        old, new = summarize(before, metric), summarize(rows, metric)
        if new["avg"] > old["avg"] + 1e-4:
            errors.append(f"{metric} MAE worsened: {old['avg']:.3f} -> {new['avg']:.3f} yd")
    old, new = summarize(before, "roll"), summarize(rows, "roll")
    if abs(new["worst"]) > 0.75 * abs(old["worst"]) + 1e-4:
        errors.append(f"Worst rollout must improve 25%: {abs(old['worst']):.3f} -> {abs(new['worst']):.3f} yd")
    for group in sorted({r["class"] for r in rows}):
        old = summarize([r for r in before if r["class"] == group], "roll")
        new = summarize([r for r in rows if r["class"] == group], "roll")
        if new["avg"] > old["avg"] + 0.5 + 1e-4:
            errors.append(f"{group} rollout MAE worsened by more than 0.5 yd: {old['avg']:.3f} -> {new['avg']:.3f}")
    return errors


def validate_provenance(results_path, library_dir):
    path = results_path.with_suffix(".manifest.json")
    if not path.exists():
        return ["No verified run manifest; rerun through shot_compare.py"]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, OSError):
        return ["Unreadable run manifest; rerun through shot_compare.py"]
    if data.get("fingerprint") != fingerprint(library_dir) or data.get("results_sha256") != hashlib.sha256(results_path.read_bytes()).hexdigest():
        return ["Stale results: source, reference, binary or result fingerprint changed"]
    return []


def run_engine(build):
    """Builds (optionally) and runs the replay test; returns the test's exit code."""
    if platform.system() == "Windows":
        base = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SCRIPTS_DIR / "ue.ps1")]
        build_command, test_command = base + ["Build"], base + ["Test", "-Filter", TEST_NAME]
    else:
        base = ["bash", str(SCRIPTS_DIR / "ue.sh")]
        build_command, test_command = base + ["build"], base + ["test", "--filter", TEST_NAME]
    if build and subprocess.call(build_command, cwd=ROOT) != 0:
        sys.exit("Editor build failed; see Saved/Logs/.")
    before = fingerprint()
    build_manifest = RESULTS_PATH.parent / "build.manifest.json"
    if build:
        build_manifest.parent.mkdir(parents=True, exist_ok=True)
        build_manifest.write_text(json.dumps({"fingerprint": before}), encoding="utf-8")
    elif not build_manifest.exists() or json.loads(build_manifest.read_text(encoding="utf-8")).get("fingerprint") != before:
        sys.exit("Source/reference/binary changed or build provenance is missing; rerun with --build.")
    if RESULTS_PATH.exists():
        RESULTS_PATH.unlink()
    RESULTS_PATH.with_suffix(".manifest.json").unlink(missing_ok=True)
    code = subprocess.call(test_command, cwd=ROOT)
    if not RESULTS_PATH.exists():
        sys.exit(f"{TEST_NAME} did not write {RESULTS_PATH}; see the newest report under Saved/Automation/.")
    if code == 0 and before == fingerprint():
        RESULTS_PATH.with_suffix(".manifest.json").write_text(json.dumps({
            "fingerprint": before, "results_sha256": hashlib.sha256(RESULTS_PATH.read_bytes()).hexdigest(),
            "created": datetime.now().isoformat(), "conditions": "flat fairway, 75 F, sea level, no wind",
        }, indent=2), encoding="utf-8")
    return code


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--skip-run", action="store_true", help="report on the last results without running Unreal")
    parser.add_argument("--build", action="store_true", help="build the editor before running (after C++ changes)")
    parser.add_argument("--session", action="append", help="only this session, such as square_20261113 (repeatable)")
    parser.add_argument("--sort", choices=[metric for metric, _, _ in METRICS], help="largest |+/-| of this metric first")
    parser.add_argument("--profile", default="current", help="profile for the per-shot table (current, historical or a candidate)")
    parser.add_argument("--baseline", type=Path, help="history snapshot to compare with instead of the last different run")
    parser.add_argument("--check-baseline", type=Path, help="enforce accuracy gates against this baseline results.csv, including provenance")
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
        return [row for row in rows if not args.session or row["session"] in args.session]

    rows_by_profile = {profile: selected(compared[profile][0]) for profile in profiles}
    table_rows = list(rows_by_profile[args.profile])
    if args.sort:
        key = "diff_" + args.sort
        table_rows.sort(key=lambda row: -1.0 if row[key] is None else abs(row[key]), reverse=True)

    skipped = sum(shot["ref"]["total"] is None for shot in selected(shots))
    print(f"Engine ({args.profile} profile) vs reference: engine value, reference value and +/- (engine minus"
          f" reference; + is longer, higher or steeper). Yards; apex in feet; landing angle in degrees.")
    print(shot_table(table_rows))
    print()
    print(f"Summary over {len(rows_by_profile['current'])} shots ({skipped} library shots have no reference total and are"
          f" not compared). Within columns count |+/-| in each metric's unit.")
    print(summary_table(rows_by_profile))
    print()
    print("Roll by launch class:")
    print(class_table(rows_by_profile))
    print()
    print("By ball speed (current profile):")
    print(speed_table(rows_by_profile["current"]))
    if any(row["landing"] is not None for row in rows_by_profile["current"]):
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
    if args.check_baseline:
        errors = validate_provenance(args.results, args.library)
        errors += validate_accuracy(shots, results, load_results(args.check_baseline))
        for error in errors:
            print("CHECK FAILED: " + error, file=sys.stderr)
        if errors:
            exit_code = 1
        else:
            print("Accuracy and provenance checks passed.")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
