# SPDX-License-Identifier: MIT
"""Flight accuracy per reference session: carry, apex and landing angle of each profile against the references.

No simulator replica and no fitting: every engine number comes from Unreal (UOpenGolf.Physics.ShotLibrary;
run python Assets/Scripts/shot_compare.py first). Sessions are read from the repository's recordings and the
local reference data (data_paths.py); only shots with a reference carry are compared, and nothing reads a club.

For each session it prints bias and mean absolute error per profile, the candidate's carry error by ball
speed, total spin and launch angle, and, with --sample-session, a fixed random sample of that session.
"""
import argparse
import random
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shot_compare import LIBRARY_DIRS, RESULTS_PATH, load_library, load_results  # noqa: E402

METRICS = (("carry", "carry yd"), ("apex", "apex ft"), ("landing", "landing deg"))
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


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--results", type=Path, default=RESULTS_PATH)
    parser.add_argument("--profile", default="current", help="candidate profile (default current)")
    parser.add_argument("--baseline-profile", default="legacy_calibrated", help="profile to compare with")
    parser.add_argument("--sample-session", help="print a fixed random sample of this session")
    parser.add_argument("--sample-size", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20261117)
    parser.add_argument("--example", action="append", default=[], help="shot also listed after the sample (repeatable)")
    args = parser.parse_args(argv)

    shots = [shot for shot in load_library(LIBRARY_DIRS) if shot["ref"]["carry"] is not None]
    results = load_results(args.results)
    sessions = list(dict.fromkeys(shot["session"] for shot in shots))
    profiles = [args.baseline_profile, args.profile]
    print("Engine minus reference: bias / mean absolute error. Each shot in its session's reference air, flat fairway, no wind.\n")
    for session in sessions:
        members = [shot for shot in shots if shot["session"] == session]
        print(f"== {session} ({len(members)} shots with a reference carry)")
        for profile in profiles:
            cells = [f"{label} {summary(errors(members, results, profile, metric))}" for metric, label in METRICS]
            print(f"  {profile:<20} " + " | ".join(cells))
        for _, title, key, edges in BANDS:
            cells = []
            for index in range(len(edges)):
                low = 0 if index == 0 else edges[index - 1]
                group = [shot for shot in members if low <= shot[key] < edges[index]]
                values = errors(group, results, args.profile, "carry")
                if values:
                    cells.append(f"{band_label(edges, index)} n={len(values)} {summary(values)}")
            print(f"    {args.profile} carry by {title}: " + "; ".join(cells))
        print()

    if args.sample_session:
        members = [shot for shot in shots if shot["session"] == args.sample_session]
        if not members:
            sys.exit(f"No session {args.sample_session} with reference carries.")
        sample = random.Random(args.seed).sample(members, min(args.sample_size, len(members)))
        sample += [shot for shot in members if shot["shot"] in args.example]
        print(f"Sample of {args.sample_session} (seed {args.seed}); examples follow it. Yards.\n")
        print(f"| Shot | Reference carry | {args.baseline_profile} | +/- | {args.profile} | +/- |")
        print("|---|---:|---:|---:|---:|---:|")
        for shot in sample:
            reference = shot["ref"]["carry"]
            old = results[(shot["session"], shot["shot"], args.baseline_profile)]["carry"]
            new = results[(shot["session"], shot["shot"], args.profile)]["carry"]
            print(f"| {shot['shot']} | {reference:.1f} | {old:.1f} | {old - reference:+.1f} | {new:.1f} | {new - reference:+.1f} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
