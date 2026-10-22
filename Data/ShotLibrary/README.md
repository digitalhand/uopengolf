# Shot library

Recorded launch-monitor shots with FlightScope's results for the same launches. The
`UOpenGolf.Physics.ShotLibrary` test replays every shot through the OpenFairway physics,
and `Scripts/flightscope_compare.py` prints how far the engine is from FlightScope.

## Table of contents

- [Running the comparison](#running-the-comparison)
- [Contents](#contents)
- [File format](#file-format)
- [Adding a session](#adding-a-session)
- [Provenance](#provenance)

## Running the comparison

Build the editor first (`Scripts/ue.ps1 Build`), then from the repository root:

```powershell
python Scripts/flightscope_compare.py            # replay every shot, then print the table
python Scripts/flightscope_compare.py --build    # build the editor first, after C++ changes
python Scripts/flightscope_compare.py --skip-run --session session_5 --club driver --sort carry
```

`+/-` is engine minus FlightScope: `+` means the engine carries, rolls or peaks higher
than FlightScope. Shots are simulated on fairway at 75 °F and sea level (the game's
defaults) with two profiles: `current` (the engine as it plays now) and `historical`
(the engine before the spin-decay and calibration-interpolation changes). Status uses
openfairway's grades: pass within 3 yd carry and 5 yd total, moderate up to 7 and 10 yd,
severe beyond. Each run with changed results is saved under `Saved/ShotLibrary/history/`,
and the report shows the change since the last different run.

## Contents

| Folder | Source | Shots | With FlightScope results |
| --- | --- | ---: | ---: |
| `standard/` | `SOT/fs_SoT.csv` (standard test shots) | 22 | 20 |
| `session_2/` | `shot_session_2/fs.csv` | 27 | 18 |
| `session_3/` | `shot_session_3/fs.csv` | 69 | 61 |
| `session_4/` | `shot_session_4/fs.csv` | 52 | 30 |
| `session_5/` | `shot_session_5/fs.csv` | 52 | 46 |

Shot names in sessions 3 to 5 include the club (`shot_7i_26` is a 7 iron); session 2 and
the standard shots are reported as club `other`.

## File format

Each session is a folder holding one `fs.csv`:

```text
shot_name,filename,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,backspin_rpm,sidespin_rpm,carry_yd,total_yd,rollout_yd,apex_ft
```

- Launch inputs: ball speed (mph), vertical and horizontal launch (degrees), total spin
  (rpm), spin axis (degrees), back spin and side spin (rpm). Positive horizontal launch,
  spin axis and side spin are to the right. All seven inputs are replayed.
- FlightScope results: carry, total and roll (yd) and apex (ft). `0.0` means FlightScope
  has no result for that shot; it is replayed but not compared. Roll is recomputed as
  total minus carry, because the standard shots record `rollout_yd` as 0.0.
- `filename` names the original shot JSON in the source session; it is not needed here.

## Adding a session

1. Create a folder such as `Data/ShotLibrary/session_6/` and put the session's `fs.csv`
   in it, unchanged. The folder name becomes the session name in reports.
2. Run `python Scripts/flightscope_compare.py`. No rebuild is needed: the test finds
   every `fs.csv` under this folder at run time.

Shot names must be unique within a session.

## Provenance

Copied unchanged on 2026-10-14 from the maintainer's
[openfairway](https://github.com/digitalhand/openfairway) repository, `assets/data` at
commit `4f3e11b` (the ogolf-physics copy is identical). Launch inputs are the maintainer's
recorded launch-monitor shots. The results come from the
[FlightScope Trajectory Optimizer](https://trajectory.flightscope.com/) for those inputs,
collected by openfairway's `tools/shot_calibration` scraper. No FlightScope code or assets
are included.
