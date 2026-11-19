# Comparison data

Data for checking the physics against reference ball flights. Nothing here is loaded by the game.

## Table of contents

- [Where the data lives](#where-the-data-lives)
- [Shot library](#shot-library)
- [File format](#file-format)
- [Adding a session](#adding-a-session)
- [Provenance](#provenance)

## Where the data lives

The repository holds only uopengolf's own shot recordings, in `ShotLibrary/recorded_<date>/`.
Third-party reference data (launch-monitor app exports, published tour averages, trajectory
calculator output), the accuracy baselines and research papers stay **outside the repository**, in
a local data folder:

- `UOPENGOLF_DATA` names the folder; by default it is `~/data/uopengolf`
  (`%USERPROFILE%\data\uopengolf` on Windows). `Assets/Scripts/data_paths.py` and
  `UOpenGolf.Physics.ShotLibrary` resolve it the same way.
- `.gitignore` keeps such data out of `Assets/Data/ShotLibrary/` (except `recorded_*`),
  `Assets/Data/ShotLibraryBaselines/` and `Docs/papers/`.

```text
<local data>/
  README.md                 sources of everything below
  shot_library/<session>/   shots.csv (and export.csv for an imported export), replayed like the repository's sessions
  references/               raw source files the sessions were made from
  baselines/                accuracy baselines for shot_compare.py --check-baseline
  papers/                   research papers cited in the physics docs
```

Without the local folder, the replay test and the scripts use the repository's recordings only and
say so; nothing fails.

## Shot library

A session is a folder holding a `shots.csv`. The `UOpenGolf.Physics.ShotLibrary` test replays every
shot of every session (repository and local) through the headless simulator on flat fairway, in the
session's reference air, and `Assets/Scripts/shot_compare.py` reports the engine against the
reference results:

```powershell
python Assets/Scripts/shot_compare.py            # replay every shot, then print the report
python Assets/Scripts/shot_compare.py --build    # build the editor first, after C++ changes
python Assets/Scripts/shot_compare.py --skip-run --session recorded_20261113 --sort roll
python Assets/Scripts/carry_audit.py             # flight (carry, apex, landing) per session
```

`+/-` is engine minus reference: `+` means the engine carries, rolls or peaks higher, or lands
steeper. Roll and total are graded only for shots with a reference total; sessions that give only a
carry (and apex or landing angle) are flight references for `carry_audit.py`. See the
[physics reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#turf-rollout-calibration)
for what the turf and the flight were fitted to.

`recorded_20261113` keeps the launch inputs of the range shots that showed the rollout faults, with
the original recording JSON under `recordings/`. It has no reference results: the report replays
those shots and checks that they come to rest but does not compare them, and the engine distances
saved in the recordings are not treated as truth. `Assets/Scripts/recording_import.py <recordings
folder> <session folder>` writes such a session.

**Club labels are not data.** The club in an export is what the app had selected, and the club in a
recording is what the game had selected; neither is measured, and both are often wrong.
`shots.csv` keeps the label as `reported_club` and in the shot names, which only identify rows. The
physics, the replay test, the calibration and the reports group and compare shots by their launch
values only. Shots struck into the ground (launch at or below 0°, no carry) are topped shots: they
are scored like every other row (see the
[physics reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#topped-shots)).

## File format

```text
shot_name,reported_club,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,backspin_rpm,sidespin_rpm,carry_yd,total_yd,rollout_yd,apex_ft,landing_angle_deg,offline_yd[,temperature_f,humidity,pressure_psi]
```

- Launch inputs: ball speed (mph), vertical and horizontal launch (degrees), total spin (rpm), spin
  axis (degrees), back spin and side spin (rpm). Positive horizontal launch, spin axis, side spin and
  offline are to the right. The test replays all seven inputs.
- Reference results: carry, total and roll (yd), apex (ft), landing angle (degrees) and offline
  (yd). A carry of 0 is a real value (a ball struck along the ground); an empty cell means no result.
- Optional air: `temperature_f`, `humidity` (0–1) and `pressure_psi`, for references computed in
  other conditions. Without them a shot is replayed at 75 °F, dry, sea level.
- `reported_club` is the selected club, kept for reference and never read.
- `shot_name` identifies the row within its session.

## Adding a session

A launch-monitor app's CSV export:

```powershell
python Assets/Scripts/monitor_import.py "C:\path\to\export.csv" [--session NAME]
python Assets/Scripts/shot_compare.py
```

The importer copies the export unchanged into `<local data>/shot_library/<session>/export.csv`
and writes `shots.csv` next to it. The session is named `export_<yyyymmdd>` from the export's date
line unless `--session` is given, and an export without the expected columns is refused. The export
marks direction with an `L` or `R` prefix (`L` is left, negative here), its `Apex(yd)` column holds
feet, and its `Average` and `Deviation` rows are skipped. No rebuild is needed: the test finds every
`shots.csv` at run time. Any other source can be added by writing a `shots.csv` in the format above.

## Provenance

`recorded_*` sessions are uopengolf's own recordings. Everything in the local data folder is
described in that folder's README; none of it is distributed with the repository. A launch-monitor
app's carry, total, apex, offline and landing angle are that app's computed results for the measured
launch, so they are another model's output, not measured ball flight.
