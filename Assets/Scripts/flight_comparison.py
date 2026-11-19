# SPDX-License-Identifier: MIT
"""Compare two Unreal shot-library runs against every carry reference (Markdown)."""
import argparse
import statistics
from pathlib import Path
from shot_compare import LIBRARY_DIRS, load_library, load_results


def report(before, after, shots):
    lines = ["# Flight calibration comparison", "", "Errors are engine minus reference; carry in yards, apex in feet.", ""]
    sessions = sorted({s['session'] for s in shots}, key=lambda s: (s not in ('fs_trajectory_calculator', 'tm_tour_averages'), s))
    lines += ["| Reference session | Before carry MAE | After carry MAE | After bias | After worst absolute error |",
              "|---|---:|---:|---:|---:|"]
    for session in sessions:
        members = [s for s in shots if s['session'] == session and s['ref']['carry'] is not None]
        if not members:
            continue
        errors = [[run[(session, s['shot'], 'current')]['carry'] - s['ref']['carry'] for s in members] for run in (before, after)]
        lines.append(f"| {session} | {statistics.mean(map(abs, errors[0])):.2f} | {statistics.mean(map(abs, errors[1])):.2f} | {statistics.mean(errors[1]):+.2f} | {max(map(abs, errors[1])):.2f} |")
    lines += [""]
    for session in sessions:
        members = [s for s in shots if s['session'] == session and s['ref']['carry'] is not None]
        if not members:
            continue
        lines += [f"## {session}", "", "| Shot | Reference | Before | Error | After | Error | Apex ref | Apex before | Apex after |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        for s in members:
            key = (session, s['shot'], 'current')
            b, a = before[key], after[key]
            ref = s['ref']['carry']
            apex = '-' if s['ref']['apex'] is None else f"{s['ref']['apex']:.1f}"
            lines.append(f"| {s['shot']} | {ref:.1f} | {b['carry']:.1f} | {b['carry']-ref:+.1f} | {a['carry']:.1f} | {a['carry']-ref:+.1f} | {apex} | {b['apex']:.1f} | {a['apex']:.1f} |")
        lines += [""]
    lines += ["## Changes for every replayed shot", "", "| Session / shot | Carry change yd | Apex change ft | Landing change deg | Roll change yd |", "|---|---:|---:|---:|---:|"]
    for s in shots:
        key = (s['session'], s['shot'], 'current')
        b, a = before[key], after[key]
        delta = lambda metric: '-' if a[metric] is None or b[metric] is None else f"{a[metric]-b[metric]:+.3f}"
        lines.append(f"| {s['session']} / {s['shot']} | {delta('carry')} | {delta('apex')} | {delta('landing')} | {delta('roll')} |")
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', required=True, type=Path)
    parser.add_argument('--after', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.write_text(report(load_results(args.before), load_results(args.after), load_library(LIBRARY_DIRS)), encoding='utf-8')
