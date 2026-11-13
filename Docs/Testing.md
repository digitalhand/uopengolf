# Testing

Run checks for the behavior you changed. Extend an existing suite when possible;
add a test only when it protects a distinct failure. Avoid tests that simply repeat
implementation details. Keep physics fixtures and meaningful regression cases.

## Before running Unreal

Build the matching editor configuration. Development builds do not update DebugGame
modules. Run only one editor, including automation processes. Let the owner close an
interactive editor before starting an automated one. Use the public map:
`/Game/UOpenGolf/Maps/UOpenGolf`.

## Headless checks

Set `UE_ROOT` and run from the repository root:

```bash
bash Assets/Scripts/ue.sh build
bash Assets/Scripts/ue.sh smoke
bash Assets/Scripts/ue.sh test --filter UOpenGolf.Range.AutoReset
```

On Windows, use `Assets/Scripts/ue.ps1 Build`, `Smoke`, or `Test -Filter <prefix>` through
PowerShell. Headless runs use NullRHI, so they do not validate rendering. Automation passes
`-NoMainMenu` to enter Play directly and use default preferences. Reports and
logs go to a fresh directory under `Saved/Automation/`. The report must contain
matching completed tests; an empty or failed run is not a pass.

| Changed behavior | Existing suite |
| --- | --- |
| Startup | `UOpenGolf.Smoke` |
| Flight, bounce, roll, stimp or contact | `UOpenGolf.Physics` and the affected range integration tests |
| Input, shot recording, reset or scoring | Relevant tests under `UOpenGolf.Range` |
| Bluetooth or Open Connect | `UOpenGolf.LaunchMonitor`; hardware checks remain separate |
| Terrain, flag cloth or vegetation wind | `UOpenGolf.Course` and `UOpenGolf.Range.CoastalTerrain` |

Install the script-test dependencies into your Python environment, then discover each
suite from the repository root (`python` can replace `python3` on Windows):

```bash
python3 -m pip install -r Assets/Scripts/requirements-test.txt
python3 -m unittest discover -s Assets/Scripts -p 'test_*.py'
python3 -m unittest discover -s Plugins/OpenGolfCourse/Scripts -p 'test_*.py'
python3 -m unittest discover -s Plugins/OpenGolfSavanna/Scripts -p 'test_*.py'
python3 -m unittest discover -s Plugins/OpenGolfSavanna/Scripts/blender -p 'test_*.py'
```

These tests exercise helpers without importing assets or rebuilding the map.
A full suite is useful after shared infrastructure changes; a focused suite is enough
for an isolated change once its results are understood.

## Rendered and hardware checks

`UOpenGolf.Visual.*` tests require rendering and are excluded from NullRHI runs.
Use the relevant rendered test and inspect its images. A successful screenshot request
alone is not visual evidence. `Assets/Scripts/check_range_render.py` runs range/flag captures
or tracer captures with `--shot-trail`.

Use `UOpenGolf.Visual.CoastalTerrain` for terrain and vegetation stability. Compare
stationary Play sequences with grass on and off using `Assets/Scripts/grass_stability.py
--profile coastal`, then inspect slow camera motion. Keep the camera, resolution,
AA settings and editor layout fixed. See [Range assets](RangeAssets.md) for wind checks
and [phase status](PhasesStatus.md) for baseline measurements and pending acceptance.

Bluetooth hardware tests require an explicit device run. Replay success does not
establish connection, reconnect or shutdown behavior on hardware. Likewise, headless
package startup does not establish rendered performance.

## Evidence

Record the engine version, OS, selected tests, report location and any limits in
[phase status](PhasesStatus.md). Keep historical measurements dated. Do not use a result
from before a change as proof of that change.

This checkout uses UnrealBuildTool, including for `.Build.cs` and `.Target.cs` files.
The optional C# reference harness is described in the
[physics reference](../Plugins/OpenFairwayPhysics/Docs/Reference.md#regenerating-fixtures).
