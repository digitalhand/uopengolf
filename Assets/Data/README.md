# Comparison data

Reference data for checking the physics against a real launch monitor. Nothing here is
loaded by the game.

## Table of contents

- [Shot library](#shot-library)
- [File format](#file-format)
- [Adding a session](#adding-a-session)
- [Provenance](#provenance)

## Shot library

`ShotLibrary/<session>/` holds one launch-monitor session. The
`UOpenGolf.Physics.ShotLibrary` test replays every shot in it through the headless
simulator (flat fairway, 75 °F, sea level), and `Assets/Scripts/shot_compare.py` reports
the engine against the launch monitor's own results:

```powershell
python Assets/Scripts/shot_compare.py            # replay every shot, then print the report
python Assets/Scripts/shot_compare.py --build    # build the editor first, after C++ changes
python Assets/Scripts/shot_compare.py --skip-run --session square_20261113 --sort roll
```

`+/-` is engine minus reference: `+` means the engine carries, rolls or peaks higher, or
lands steeper. See the
[physics reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#turf-rollout-and-square-calibration)
for what the current turf was fitted to.

| Session | Source | Shots | Launch values |
| --- | --- | ---: | --- |
| `square_20261113` | Square Home export, Driving Ranges, 2026-11-13 21:32 | 124 | 36–121 mph, −1.8° to 28.5°, 1134–8794 rpm |
| `recorded_20261113` | UOpenGolf range recordings, 2026-11-13 20:56–21:30 | 36 | Launch inputs only; no reference results |

`recorded_20261113` keeps the launch inputs of the range shots that showed the rollout faults, with
the original recording JSON under `recordings/`. It has no reference results: the report replays
those shots and checks that they come to rest but does not compare them, and the engine distances
saved in the recordings are not treated as truth. `Assets/Scripts/recording_import.py <recordings
folder> <session folder>` writes such a session.

**Club labels are not data.** The club in a Square export is what the app had selected, and the
club in a recording is what the game had selected; neither is measured (Square Home exports club
speed and smash factor as 0), and the 2026-11-13 labels are wrong: those shots were hit with irons.
`shots.csv` keeps the label as `reported_club` and in the shot names, which only identify rows.
The physics, the replay test, the calibration and `shot_compare.py` group and compare shots by
their launch values only. Nine Square shots were struck along the ground (launch below 2.5°, six of
them below 0° with no carry). The six with no carry are topped shots: they are scored like every
other row, and the turf's struck-along-the-ground grass drag is fitted to them (see the
[physics reference](../../Plugins/OpenFairwayPhysics/Docs/Reference.md#topped-shots)).

## File format

Each session folder holds:

- `square_export.csv`: the export exactly as the Square app wrote it.
- `shots.csv`: the normalized copy the test and report read, written by
  `Assets/Scripts/square_import.py`.

```text
shot_name,reported_club,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,backspin_rpm,sidespin_rpm,carry_yd,total_yd,rollout_yd,apex_ft,landing_angle_deg,offline_yd
```

- Launch inputs: ball speed (mph), vertical and horizontal launch (degrees), total spin
  (rpm), spin axis (degrees), back spin and side spin (rpm). Positive horizontal launch,
  spin axis, side spin and offline are to the right. The test replays all seven inputs.
- Reference results: carry, total and roll (yd), apex (ft), landing angle (degrees) and
  offline (yd). A carry of 0 is a real value (a ball struck along the ground); an empty cell
  means no result.
- `reported_club` is the selected club, kept for reference and never read.
- `shot_name` is that label without spaces and the export's index, such as `7iron_08`; the
  export numbers rows within each label, so the label is part of the identifier, nothing more.

The Square export marks direction with an `L` or `R` prefix (`L` is left, negative here).
Its `Apex(yd)` column holds feet: a 108 mph, 12° shot that carries 150 yd peaks at 46.4,
which is feet.
`Average` and `Deviation` rows are skipped.

## Adding a session

```powershell
python Assets/Scripts/square_import.py "C:\path\to\export.csv"
python Assets/Scripts/shot_compare.py
```

The importer names the session `square_<yyyymmdd>` from the export's date line (pass
`--session NAME` to choose another) and refuses an export without the expected columns.
Shot names must be unique within a session. No rebuild is needed: the test finds every
`shots.csv` at run time.

## Provenance

The maintainer's own Square Home sessions, exported from the Square app. The launch data
was measured by the device; carry, total, apex, offline and landing angle are the Square
app's computed results for those launches, so they are another model's output, not
measured ball flight. No Square code or assets are included.
