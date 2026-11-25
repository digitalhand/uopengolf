# Testing

Run checks for the behavior you changed. Extend an existing suite when possible;
add a test only when it protects a distinct failure. Avoid tests that simply repeat
implementation details. Keep the physics pins and meaningful regression cases.

## Before running Unreal

Build the matching editor configuration. Development builds do not update DebugGame
modules. Run only one editor, including automation processes. Let the owner close an
interactive editor before starting an automated one. Use the public map:
`/Game/UOpenGolf/Maps/L_SavannaRange`.

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
| Flight, bounce, roll, stimp or contact | `UOpenGolf.Physics` (its `GameplayPins` must not move unless the physics is meant to change) and the affected range integration tests |
| Turf roll or flight accuracy against reference shots | Needs the local reference data ([Assets/Data](../Assets/Data/README.md#where-the-data-lives)): `python Assets/Scripts/shot_compare.py --build --check-baseline <local data>/baselines/pre_contact_20261116.csv`, then `python Assets/Scripts/carry_audit.py --before <earlier results.csv>`; see [physics accuracy scripts](Development.md#physics-accuracy-scripts) |
| Training games and finished balls | `UOpenGolf.Range.TrainingGames`. In rendered Play, `UOpenGolf.Visual.RangeHudControls` opens the game picker and drill dialog, and `UOpenGolf.Visual.TrainingTarget` captures tee, target and moving-camera views. |
| Input, shot recording, reset or scoring | Relevant tests under `UOpenGolf.Range` |
| Bluetooth or Open Connect | `UOpenGolf.LaunchMonitor`; hardware checks remain separate |
| Terrain, flag cloth or vegetation wind | `UOpenGolf.Course` and `UOpenGolf.Range.Terrain` |
| Content names, folders or actor labels (any new or renamed asset) | `python3 Assets/Scripts/check_content_names.py` and `UOpenGolf.Content.Naming`; see [Content naming](ContentNaming.md) |
| Course definitions, round scoring or travel | `UOpenGolf.Course` (its `Catalog` test checks the bundled course against the range) and `UOpenGolf.Range.HolePlay`; with rendering (`-Rendered`), `UOpenGolf.Visual.CourseBrowser` |
| Round setup (tabs, course information, random wind, picked holes, scorecard) | `UOpenGolf.Course.Play.RoundSetup` (headless rules and the wind roll), `UOpenGolf.Menu.RoundSetup` (the page through its buttons), `UOpenGolf.Course.HunterRanch.BackNine`, `.PickedHoles` and `.RolledWind` (Play), and with rendering `UOpenGolf.Visual.CourseBrowser` (800×600 through 2560×1080, tab and footer hit tests, and large-text captures with Hunter Ranch installed) |
| Course building scripts | `python3 -m unittest discover -s Plugins/OpenGolfCourse/Scripts -p 'test_*.py'` (layout, elevation, folders) |
| Fast-forward (Space) | `UOpenGolf.Course.Play.FastForward` (tap and hold rules), `UOpenGolf.Range.FastForward` (Play), `UOpenGolf.Range.FrameRates` (the dilated step cap) and `UOpenGolf.Range.InputBindings` |
| Fairway speed or Gimmie choices | `UOpenGolf.Physics.FairwayStimp`, `UOpenGolf.Menu.PreferenceValidation` and `UOpenGolf.Menu.GimmieScoring` |
| Hole minimap | `UOpenGolf.Minimap` (framing, ground fields, compositing and labels run headless; `RenderedClick` needs rendering, `-Rendered`) and `UOpenGolf.Range.Aim` for the map's target |
| In-game updates | `OpenGameUpdate` (run with `-Filter OpenGameUpdate`); the installer by hand, see [In-game updates](Releases.md#in-game-updates) |
| Ubuntu packaging | `python3 -B -m unittest Assets.Scripts.test_build_deb Assets.Scripts.test_check_package_runner`; requires `dpkg-deb` |

For Ubuntu package installation checks, run `Assets/Scripts/check_deb_install.sh` only inside
a disposable Ubuntu 24.04 Docker container. Mount this checkout read-only at `/src`, the
directory containing the `.deb` read-only at `/packages`, and a report directory at `/reports`.
Run `bash /src/Assets/Scripts/check_deb_install.sh /packages/UOpenGolf-<version>-Linux-amd64.deb`.
The check installs dependencies inside the container, tests upgrades and removal with small
fixtures, then installs the real game and starts it as an ordinary user for 60 seconds with
NullRHI. It checks all shipped ELF dependencies, validates the AppStream metadata and checks
that AppStream links it to the package. It also preserves sample user settings through
removal and purge. Graphical installation and menu launch require a separate desktop check.

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

Use `UOpenGolf.Visual.RangeTerrain` for terrain and vegetation stability. Compare
stationary Play sequences with grass on and off using `Assets/Scripts/grass_stability.py
--profile terrain`, then inspect slow camera motion. Keep the camera, resolution,
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
