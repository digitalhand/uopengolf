# SPDX-License-Identifier: MIT
"""Validate a separately installed range map, or capture its presentation.

Requires a built editor. Asset paths are supplied by the caller; public code has
no dependency on a particular private pack. Reports stay under Saved/Automation.
"""
import argparse
from datetime import datetime
import os
from pathlib import Path
import subprocess
import sys
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', required=True, help='Unreal package path of the range map')
    parser.add_argument('--engine-root', default=os.environ.get('UE_ROOT'))
    parser.add_argument('--render', action='store_true', help='Capture views and relative editor frame times')
    parser.add_argument('--shot-trail', action='store_true', help='Capture moving shots instead of static range views (requires --render)')
    parser.add_argument('--trail-timing', action='store_true', help='Measure rendered shots without screenshot overhead (requires --shot-trail)')
    parser.add_argument('--legacy-trail', action='store_true', help='Measure previous tracer mesh implementation for comparison (requires --trail-timing)')
    parser.add_argument('--static-only', action='store_true', help='Skip stationary frame sequences; retain seven views and frame timings')
    parser.add_argument('--timeout', type=int, default=600)
    args = parser.parse_args()
    if args.shot_trail and not args.render:
        parser.error('--shot-trail requires --render')
    if args.trail_timing and not args.shot_trail:
        parser.error('--trail-timing requires --shot-trail')
    if args.legacy_trail and not args.trail_timing:
        parser.error('--legacy-trail requires --trail-timing')
    if not args.map.startswith('/') or any(c in args.map for c in '\";\r\n '):
        parser.error('--map must be an Unreal package path without spaces or command separators')
    if not args.engine_root:
        parser.error('Set UE_ROOT or supply --engine-root')
    root = Path(__file__).resolve().parent.parent
    engine = Path(args.engine_root)
    binary = engine / ('Engine/Binaries/Win64/UnrealEditor-Cmd.exe' if os.name == 'nt' else 'Engine/Binaries/Linux/UnrealEditor')
    prefix = 'UOpenGolf.Visual.RangeAssets' if args.render else 'UOpenGolf.OptionalRange'
    if args.shot_trail:
        prefix = 'UOpenGolf.Visual.ShotTrail'
    report = root / 'Saved/Automation' / (datetime.now().strftime('%Y%m%d-%H%M%S') + '-variant-' + uuid.uuid4().hex[:8])
    report.mkdir(parents=True)
    command = [str(binary), str(root / 'UOpenGolf.uproject'), '-unattended', '-nopause', '-nosplash', '-nosound',
               '-DDC=UOpenGolfSmoke', f'-ExecCmds=Automation RunTests {prefix}',
               '-TestExit=Automation Test Queue Empty', f'-ReportExportPath={report}',
               f'-abslog={report / "Editor.log"}', f'-RangeVariantMap={args.map}', f'-RangeVisualMap={args.map}']
    command += ['-RenderOffscreen', '-ResX=1280', '-ResY=720'] if args.render else ['-nullrhi']
    if args.trail_timing:
        command = [arg for arg in command if not arg.startswith('-ExecCmds=')]
        command += ['-RangeTrailTimingOnly', '-ExecCmds=r.VSync 0,t.MaxFPS 120,Automation RunTests UOpenGolf.Visual.ShotTrail']
    if args.legacy_trail:
        command += ['-RangeLegacyTrail']
    if args.static_only:
        command += ['-RangeStaticOnly']
    print(f'Range verification report: {report}', flush=True)
    try:
        with (report / 'stdout.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
        # -TestExit forces an exit, which Unreal reports as 1 on Linux even after a clean run
        # (FUnixPlatformMisc::RequestExit), so the report decides pass/fail for 0 and 1.
        if result.returncode not in (0, 1) or not (report / 'index.json').is_file():
            raise subprocess.CalledProcessError(result.returncode, command)
        subprocess.run([sys.executable, str(root / 'Scripts/check_automation_report.py'), str(report / 'index.json'), '--prefix', prefix], check=True)
    except (subprocess.SubprocessError, OSError) as error:
        print(f'Verification failed: {error}. See {report}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
