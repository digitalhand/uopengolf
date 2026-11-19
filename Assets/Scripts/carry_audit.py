# SPDX-License-Identifier: MIT
"""Flight accuracy per reference session: carry, apex and landing angle against the references, and the change from an earlier run.

No simulator replica and no fitting: every engine number comes from Unreal (UOpenGolf.Physics.ShotLibrary;
run python Assets/Scripts/shot_compare.py first). Sessions are read from the repository's recordings and the
local reference data (data_paths.py); only shots with a reference carry are compared, and nothing reads a club.

For each session it prints bias and mean absolute error, and the carry error by ball speed, total spin and
launch angle. With --before (an earlier results.csv) it also prints that run's errors and the largest change of
any replayed shot, and --markdown writes the full before/after report: every session's carry and apex, and the
carry, apex, landing-angle and roll change of every shot. --sample-session prints a fixed random sample.
"""
import argparse
import random
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shot_library import RESULTS_PATH, load_library, load_results  # noqa: E402

METRICS = (("carry", "carry yd"), ("apex", "apex ft"), ("landing", "landing deg"))
CHANGE_METRICS = ("carry", "total", "roll", "apex", "landing")
BANDS = (
    ("speed", "ball speed mph", "speed", (75.0, 95.0, 120.0, float("inf"))),
    ("spin", "total spin rpm", "spin", (3000.0, 5000.0, float("inf"))),
    ("vla", "launch deg", "vla", (12.0, 20.0, float("inf"))),
)


def errors(shots, results, profile, metric):
    pairs = []
    for shot in shots:
        engine = results.get((shot["session"], shot["shot"], profile))
        reference = shot["ref"][metric]
        if engine is not None and reference is not None and engine[metric] is not None:
            pairs.append(engine[metric] - reference)
    return pairs


def summary(values):
    return f"{statistics.mean(values):+6.2f} / {statistics.mean(map(abs, values)):5.2f}" if values else "     -       "


def band_label(edges, index):
    low = 0 if index == 0 else edges[index - 1]
    high = edges[index]
    return f">={low:g}" if high == float("inf") else f"{low:g}-{high:g}"


def changes(shots, before, after, profile):
    """Per-shot after-minus-before of every change metric, for shots in both runs."""
    rows = []
    for shot in shots:
        key = (shot["session"], shot["shot"], profile)
        if key in before and key in after:
            rows.append((shot, {metric: None if after[key][metric] is None or before[key][metric] is None
                                else after[key][metric] - before[key][metric] for metric in CHANGE_METRICS}))
    return rows


def largest_change(rows):
    """The largest absolute change of each metric over the rows, with the shot it belongs to."""
    largest = {}
    for shot, delta in rows:
        for metric, value in delta.items():
            if value is not None and abs(value) > abs(largest.get(metric, (0.0, None))[0]):
                largest[metric] = (value, f"{shot['session']}/{shot['shot']}")
    return largest


def markdown_report(shots, before, after, profile):
    """Before/after Markdown: carry error per session, every referenced shot, and every replayed shot's change."""
    referenced = [shot for shot in shots if shot["ref"]["carry"] is not None]
    sessions = list(dict.fromkeys(shot["session"] for shot in referenced))
    lines = ["# Flight comparison", "", f"Profile {profile}. Errors are engine minus reference; carry in yards, apex in feet.", "",
             "| Reference session | Before carry MAE | After carry MAE | After bias | After worst absolute error |",
             "|---|---:|---:|---:|---:|"]
    for session in sessions:
        members = [shot for shot in referenced if shot["session"] == session]
        old, new = errors(members, before, profile, "carry"), errors(members, after, profile, "carry")
        if old and new:
            lines.append(f"| {session} | {statistics.mean(map(abs, old)):.2f} | {statistics.mean(map(abs, new)):.2f} | "
                         f"{statistics.mean(new):+.2f} | {max(map(abs, new)):.2f} |")
    for session in sessions:
        lines += ["", f"## {session}", "", "| Shot | Reference | Before | Error | After | Error | Apex ref | Apex before | Apex after |",
                  "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        for shot in (shot for shot in referenced if shot["session"] == session):
            key = (session, shot["shot"], profile)
            if key not in before or key not in after:
                continue
            reference, old, new = shot["ref"]["carry"], before[key], after[key]
            apex = "-" if shot["ref"]["apex"] is None else f"{shot['ref']['apex']:.1f}"
            lines.append(f"| {shot['shot']} | {reference:.1f} | {old['carry']:.1f} | {old['carry'] - reference:+.1f} | {new['carry']:.1f} | "
                         f"{new['carry'] - reference:+.1f} | {apex} | {old['apex']:.1f} | {new['apex']:.1f} |")
    lines += ["", "## Changes for every replayed shot", "",
              "| Session / shot | Carry yd | Total yd | Roll yd | Apex ft | Landing deg |", "|---|---:|---:|---:|---:|---:|"]
    for shot, delta in changes(shots, before, after, profile):
        cells = ["-" if delta[metric] is None else f"{delta[metric]:+.3f}" for metric in CHANGE_METRICS]
        lines.append(f"| {shot['session']} / {shot['shot']} | " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--results", type=Path, default=RESULTS_PATH)
    parser.add_argument("--profile", default="current", help="profile to report (default current)")
    parser.add_argument("--before", type=Path, help="an earlier results.csv to compare with")
    parser.add_argument("--markdown", type=Path, help="write the before/after report here (needs --before)")
    parser.add_argument("--library", type=Path, action="append", help=argparse.SUPPRESS)
    parser.add_argument("--sample-session", help="print a fixed random sample of this session")
    parser.add_argument("--sample-size", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20261117)
    parser.add_argument("--example", action="append", default=[], help="shot also listed after the sample (repeatable)")
    args = parser.parse_args(argv)
    if args.markdown and not args.before:
        parser.error("--markdown needs --before")

    library = load_library(args.library)
    shots = [shot for shot in library if shot["ref"]["carry"] is not None]
    results = load_results(args.results)
    before = load_results(args.before) if args.before else None
    runs = ([("before", before)] if before else []) + [("now" if before else args.profile, results)]
    sessions = list(dict.fromkeys(shot["session"] for shot in shots))
    print("Engine minus reference: bias / mean absolute error. Each shot in its session's reference air, flat fairway, no wind.\n")
    for session in sessions:
        members = [shot for shot in shots if shot["session"] == session]
        print(f"== {session} ({len(members)} shots with a reference carry)")
        for label, run in runs:
            cells = [f"{name} {summary(errors(members, run, args.profile, metric))}" for metric, name in METRICS]
            print(f"  {label:<8} " + " | ".join(cells))
        for _, title, key, edges in BANDS:
            cells = []
            for index in range(len(edges)):
                low = 0 if index == 0 else edges[index - 1]
                group = [shot for shot in members if low <= shot[key] < edges[index]]
                values = errors(group, results, args.profile, "carry")
                if values:
                    cells.append(f"{band_label(edges, index)} n={len(values)} {summary(values)}")
            print(f"    carry by {title}: " + "; ".join(cells))
        print()

    if before:
        rows = changes(library, before, results, args.profile)
        print(f"Change from {args.before} over {len(rows)} replayed shots (largest absolute change):")
        for metric, (value, shot) in sorted(largest_change(rows).items(), key=lambda item: CHANGE_METRICS.index(item[0])):
            print(f"  {metric:<8} {value:+.4f} ({shot})")
        if not rows:
            print("  no shot is in both runs")
        print()
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(markdown_report(library, before, results, args.profile), encoding="utf-8")
        print(f"Report: {args.markdown}")

    if args.sample_session:
        members = [shot for shot in shots if shot["session"] == args.sample_session]
        if not members:
            sys.exit(f"No session {args.sample_session} with reference carries.")
        sample = random.Random(args.seed).sample(members, min(args.sample_size, len(members)))
        sample += [shot for shot in members if shot["shot"] in args.example]
        print(f"Sample of {args.sample_session} (seed {args.seed}); examples follow it. Yards.\n")
        print("| Shot | Reference carry | " + " | ".join(f"{label} | +/-" for label, _ in runs) + " |")
        print("|---|---:|" + "---:|---:|" * len(runs))
        for shot in sample:
            reference = shot["ref"]["carry"]
            cells = []
            for _, run in runs:
                carry = run[(shot["session"], shot["shot"], args.profile)]["carry"]
                cells.append(f"{carry:.1f} | {carry - reference:+.1f}")
            print(f"| {shot['shot']} | {reference:.1f} | " + " | ".join(cells) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
