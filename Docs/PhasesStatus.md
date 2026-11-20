# Phase status

## HUD toolbar, tooltips and local time — 2026-11-19 (Windows)

Version **0.1.2**. Earlier entries describe the HUD and time zones as they were then;
[Playing](Playing.md#hud) describes them now.

- **Toolbar.** The Training HUD's status box is gone. The top-left toolbar holds the club
  picker (a drawer of every club), the Game Mode flag, and the record toggle on the range
  or the place-ball button on the hole. The top-right heading reads **Training**,
  **Training · Ball Placement** or **Airways** with the hole details, and its cog opens
  Settings › Player. The captions under the icons, the aim readout and the
  `HUD [F6 / View]` hint beside the version were removed. F6 navigation is unchanged.
- **Tooltips.** Each toolbar icon (club, flag, record, ball) names itself, with its
  key, in a hover tooltip drawn in the HUD colors: an opaque dark tile, light text and a
  thin accent outline, wrapped to 300 px. This replaces the 2026-11-11 rule that nothing in
  the game shows a tooltip; menus and panels still show none. Engine text-only tooltips draw
  black text on a bright background and ignore the border brush, so the HUD tooltip
  supplies its own text widget (`UI/Common/HudToolTip`).
- **Local time.** The **Time zone** setting was removed. Shot-history times and the
  member-since date use this computer's local time; CSV exports keep UTC. An older
  preferences file's `TimeZone` key is ignored. Club averages list the longest carry first.
- **Review fixes.**
  - Toolbar buttons had no disabled brush, so a disabled one would have drawn a white box.
  - The club drawer's focus index is now bounds-checked.
  - The play bar looked up the online service every frame; it now looks it up once.
  - The course header's fixed headings are no longer rebuilt on every text refresh.
  - The flag and ball buttons fade their icons with the widget's tint and disabled state.

VERIFICATION_PENDING

## Ubuntu Debian installer — 2026-11-19 (Linux)

The Linux release workflow now produces `UOpenGolf-<version>-Linux-amd64.deb` and its
SHA-256 alongside the portable and symbols archives. `Assets/Installer/build_deb.py`
builds without root. Installation uses Ubuntu's normal administrator authorization,
adds an application-menu entry, and keeps the game under `/opt/uopengolf`. The launcher
uses `-SaveToUserDir`, keeping settings outside package-owned files. Linux's update
button still opens the release page. See [Ubuntu installer](Releases.md#ubuntu-installer).

- Built the current 0.1.0 game with Unreal Engine **5.8.4-58902052**, Linux Development,
  on Ubuntu 26.04 amd64. Used a separate source copy under `Saved/DebPackageBuild`,
  excluding machine-specific `Config/User*.ini`. The build, cook and existing 60-second
  package startup check passed. An older cooked 0.0.7 build was discarded from the
  release output after its log exposed a version mismatch and missing wind UI.
- **48 script tests passed**, including real fixture `.deb` construction, root ownership,
  permissions, SHA-256, prerelease ordering, excluded settings/debug files, launcher
  arguments and missing-tool checks. The builder reached **99.1% line coverage** with
  Python's trace tool. ShellCheck, desktop-entry validation and workflow YAML parsing passed.
- In a disposable **Ubuntu 24.04 amd64** container, package installation, upgrade,
  obsolete-file removal, uninstall and purge passed. Sample user settings survived.
  Every shipped ELF dependency resolved. The real installed game ran as an ordinary user
  for 60 seconds, loaded the public map, passed the startup check, and reported the same
  **0.1.0** version as the package. The final checksum was verified.
- Reports: `Saved/Automation/DebianPackage/ScriptTests.log`, `Install.log`, and `Game.log`;
  build reports are under `Saved/DebPackageBuild/Saved/`. The local installer is under
  `Packaged/dist/`. No host package installation or release publication was performed.
- **Limits:** graphical package installation and application-menu launch on an Ubuntu
  desktop remain pending. NullRHI startup does not validate graphics, audio, Bluetooth,
  sign-in or FPS. The current game's existing non-fatal `GameFeatureData` class-loading
  ensure remains in the startup log; packaging did not change that configuration.

## Physics cleanup: legacy code removed, files split, docs rewritten — 2026-11-18 (Windows)

The gameplay physics is unchanged. Everything it no longer used is removed, the physics code is split by
concern, and the physics docs describe the current feature.

- **Removed.** The ported legacy flight (its drag polynomial and lift curves, launch-regime overrides and
  Euler step), the measured research-ball comparison model, `HistoricalReference()` and
  `LegacyCalibrated()` with every toggle only they used (the split-step ground solver, total-spin contact,
  the ported turf catalog, hard regime lookups, the no-air stimp solve), the C# golden-fixture harness, and
  the tests of those (`Fixtures`, `FlightDiagnostics`, `GreenReference`, `FlightCorrections`). The ground air
  drag and the stimp solve keep the drag curve they were calibrated with, as
  `FlightAerodynamics::GroundDragCoefficient`.
- **Pinned.** The new `UOpenGolf.Physics.GameplayPins` pins 27 headless shots, the stimp friction and the
  ground drag, recorded before the cleanup. `GreenPlay` keeps the green checks on the gameplay profile, and
  `LandingResponse` the landing-bucket checks.
- **Renamed.** Physics code names no ball brand: the measured drag table is `BallData.h`.
- **Split.**
  - `GolfPhysics.cpp` became Atmosphere, FlightStep, Impact and GroundRoll.
  - The ball actor became lifecycle, motion, surface, cup and presentation files.
  - The obstacle and canopy rules moved to `Obstacles`, and the steps the simulator and the actor share to `ShotStep`.
  - Unit factors moved to `Units.h`, which the game's wind HUD uses too.
  - The test helpers moved to `PhysicsTestUtils.h` and `ShotLibrary.h`, and the turf fit to `RolloutCalibrationTest.cpp`.
- **Scripts.**
  - `flight_fit.py` reads the flight table and ball constants from the engine headers and has `check`, `fit` and `refine` (the former one-off driver fit, now cohorted by launch spin ratio).
  - `carry_audit.py --before/--markdown` replaces `flight_comparison.py`.
  - `shot_library.py` holds the shared loader.
- **Docs.** The physics reference describes the current pipeline from launch to cup; `Accuracy.md` replaces
  `CarryAudit.md`, with fresh numbers.

Validation: Unreal Engine 5.8, Windows x64, Development editor built.
- `UOpenGolf` passed 113 of 113 before the cleanup (`Saved/Automation/20261118-181316-dde96588`) and 112 of 112
  after it (`Saved/Automation/20261118-185048-bf64e25c`): four legacy-only tests removed, three added.
- The shot-library replay (196 shots) is identical to before in every reported column, and `shot_compare.py`
  reports a change of +0.00 in every metric. The range tests report the same numbers, except one camera-zoom
  reading (0.001).
- `shot_compare.py --check-baseline` fails the same gate as before: mid launches at 4000 rpm and above roll
  2.3 yd off lm (1.1 yd in the baseline).
- Python: 93 engine-free tests; 89 pass, 3 skip without NumPy, and `test_grass_stability` still needs Pillow,
  which this Python lacks. With NumPy the flight-fit tests pass and `flight_fit.py check` reproduces Unreal's
  carry errors to about 0.01 yd.

Not verified:

- `flight_fit.py refine` (SciPy is not installed here);
- Linux;
- packaged builds.

## Flight model fitted to reference flights; reference data moved out of the repo — 2026-11-18 (Windows)

Full shots carried 5–10 yd long: against tour averages (tm) the flight was 10.1 yd long on average,
against the launch-monitor export (lm) 4.1 yd off per shot. See
[Accuracy.md](../Plugins/OpenFairwayPhysics/Docs/Accuracy.md) for the current results.

- **Flight model.** Gameplay flight is `EFlightModel::SpinRatioV1`: drag and lift as curves in the spin
  ratio, plus 0.68 of a modern ball's measured drag rise below Re 100k, integrated with coupled RK4.
  No club, launch-angle or regime term reaches it. The table (`SpinRatioData.h`) is fitted by
  `Assets/Scripts/flight_fit.py` to tm, lm and the trajectory calculator (fs), weighted 2 : 1 : 0.5,
  with drag never below 0.17 and the low-spin lift of a measured modern ball.
  - Carry bias / error: tm +0.3 / 2.3 yd (legacy +10.1 / 10.1), lm −2.2 / 2.2 (−1.0 / 4.1), fs −3.8 /
    3.8. Apex error against lm 0.6 ft (2.7). The men's tour-average driver carries 276.4 yd against 275.
  - `LegacyCalibrated()` keeps the previous flight, and `ProV1Reference()` the measured-coefficient
    model of one modern ball (13.3 yd error against lm), for comparison.
- **Reference data.** Third-party data (lm, tm, fs), the accuracy baseline and the research papers
  moved to a local folder outside the repository (`UOPENGOLF_DATA`, default `~/data/uopengolf`), and
  `.gitignore` keeps them out. The replay test and the scripts read both the repository's recordings
  and the local sessions, each shot in its session's air, and skip the local ones when absent. Code,
  tests and docs no longer name the sources; comparison docs use the acronyms lm, tm and fs.
  `monitor_import.py` replaces the export importer; `carry_audit.py` reports flight per session.

Validation: Unreal Engine 5.8, Windows x64, Development editor built.
- `UOpenGolf` passed 112 of 113 (`Saved/Automation/20261118-115409-8a89cd6b`), with the usual warnings.
  `UOpenGolf.Range.RecordedContactParity` fails one case: a 113.9 mph / 0.6° launch that touches down
  twice on sand ends 0.31 yd farther live than headless (0.25 yd allowed). Flight and both impacts
  agree; the live sweep finds the second, grazing contact a step late.
- `shot_compare.py --check-baseline` against the moved baseline fails one gate: mid launches above
  4000 rpm roll 2.3 yd short of lm (1.1 yd before; 0.5 yd allowed). The new flight lands about 1.3°
  steeper and slower than lm's model, and the turf was fitted to the legacy landings. Roll error
  overall is 1.8 yd, total error 3.9 yd (4.0 before).
- Every library shot's RK4 carry converges within 0.1 yd of a 960 Hz run; live and headless carry
  agree within 0.25 yd at 30, 144 and stalled 60 fps.
- 82 of 83 Python tests pass; `test_grass_stability` needs PIL, which this Python lacks.

Not verified:

- a turf refit to the new landings, and the ground step for the rest of an impact step;
- the new flight in headwinds against the published wind sets;
- flight beyond 36–175 mph;
- Linux;
- packaged builds.

## Review fixes: turf grip bound, ground wind, topped shots — 2026-11-17 (Windows)

A review of the turf landing law found three faults. All three were confirmed in the code and the
saved replay.

- **Grip without load.** The landing law's backspin term applied in full at any downward normal
  speed. A touch at 0.1 mm/s kept 68.7% of a 2 m/s, 2000 rpm ball's speed, and none at 8000 rpm.
  The term is now bounded by the contact's Coulomb limit (kinetic friction times the normal
  impulse), the same bound the friction impulse uses. It fades out as the normal speed goes to
  zero.
- **Ground wind from the launch height.** `StepGround` read the wind at the ball's height above its
  launch point. Under the 10 m profile a ball rolling 1 m above the tee got 61% of the wind, and one
  below it none. A ball on the ground now meets the wind one radius above its own lie.
  - The ground step's air drag also stopped reading the launch's regime drag multiplier (0.55–1.11)
    and high-launch drag (up to 1.24×). It uses `FFlightEnvironment::BallDragScale` and the ball's
    own velocity and spin.
- **Topped shots.** lm's six rows launched below 0° (no carry, no apex) stopped 32–47 yd short
  and failed three baseline gates. They are scored, not excluded.
  - No model that reads only the ball's state can fit them together with lm's shots that fly
    first. lm rolls 113.9 mph at +0.6° 69.2 yd after landing at 0.8°, but 103.7 mph at −0.2°,
    in nearly the same ground state, 124.6 yd.
  - A full shot struck into or along the ground now keeps its own share of the turf's grass drag
    for its whole roll (`StruckViscosityScale` 0.26, `StruckFormDragScale` 0.69).
  - The share ramps in over 12–18 m/s of launch speed, so putts are unaffected.
  - At full speed, total distance steps by about 45% between 0° and 0.1°, as lm's own results
    do. `RolloutRegression` checks continuity on each side of 0° instead of across it.
- **Refit.** `UOpenGolf.Calibration.Rollout` now fits in two stages: the turf values on the shots
  that leave the ground, then the struck drag on the topped shots.
  - A new constraint keeps rough holding a resting ball on slopes at least 1° steeper than a green,
    as `SlopeRollout` requires. An unconstrained run had dropped fairway rolling friction to 0.029.
  - Fairway: k 0.665, c 0.464, rolling friction 0.0469, grass drag 0.0242 plus 0.0086 /m speed
    squared, 7.4 ft on a stimpmeter (no longer on the 6 ft floor).
- **Against lm** (124 shots): roll error 1.23 yd (3.67 under the first 2026-11-17 fit), total
  error 3.98 yd (5.36). The six topped shots finish within 0.9 yd on average (worst 2.5 yd).
  - The three 0.6–1.6° shots that fly now roll within 0.8 yd, but carry 6–16 yd short, so they
    total 7–16 yd short.
  - Against the baseline `pre_contact_20261116.csv` (now in the local reference data), every gate of
    `shot_compare.py --check-baseline` passes; worst roll is 7.8 yd against a 25.75 yd limit.

Validation: Unreal Engine 5.8, Windows x64, Development editor built.
- `UOpenGolf.Physics` passed 21 of 21 (`Saved/Automation/20261117-145826-71a0b18f`); its one warning
  is `ShotInterrupt`'s intended stop.
- `UOpenGolf.Range` passed 36 of 36, four with the usual warnings
  (`Saved/Automation/20261117-145843-45790213`).
  - The drive totals 268.3 yd.
  - The four recorded problem launches total 81.3, 41.5, 77.4 and 110.8 yd on the range, against
    87.1, 41.5, 77.5 and 111.1 yd on flat fairway. The first, a topped shot, ends in rough on the
    range.
- The calibration ran in `Saved/Automation/20261117-145705-5a36a301`.
- All 32 Python tests for the four script test modules passed.

Not verified:

- flight: carry runs 2–4 yd short below 90 mph and 4 yd long above 105 mph;
- shots above 121 mph ball speed;
- topped shots on rough or the other turf surfaces, and on slopes;
- flight wind height, which is still measured from the launch ground;
- Linux;
- packaged builds;
- a real Square Home shot in play.

## Wind HUD in packaged builds; custom test shots — 2026-11-17 (Windows)

- **Wind HUD.** `AUOpenGolfHUD` loads `WBP_WindIndicator` only by a C++ soft path. No package referenced it,
  so the cook left it out: the 2026-11-09 package had nothing under `UOpenGolf/UI`. The packaged HUD then
  showed no wind indicator and logged nothing. `Config/DefaultGame.ini` now always cooks `/Game/UOpenGolf/UI`.
  The HUD logs an error when the widget does not load, `check_package_start.py` fails a package on that
  error, and `UOpenGolf.Smoke.UOpenGolf` checks that the packaging settings cook the widget's folder.
- **Custom test shots.** **Custom** in Test shots [P] shows five fields that start as a straight shot:
  100 mph, 16°, 0°, 5000 rpm, 0° axis. Speed is in the player's units. **H** repeats the typed shot. Values
  are limited to `LaunchMonitorLimits`, which moved to the plugin's Public folder, and last until the map
  reloads.
  - The shot goes in through the new `UOpenLaunchMonitorSubsystem::InjectCustomShot` with Source "Custom".
    It keeps the admin gate and is never recorded.
  - Back and side spin are derived by `DeriveSpin`, now shared with Open Connect in `Private/LaunchMonitorSpin.h`.
  - Keys typed into a field never reach the play keys. HUD navigation (F6 or gamepad) lets typing through to
    a field and no longer lists a text box twice.

Validation: Unreal Engine 5.8, Windows x64, Development editor and game built.

| Tests | Report (`Saved/Automation/`) |
| --- | --- |
| `UOpenGolf.LaunchMonitor`, 19 of 19, including the new `CustomShot` and all of Open Connect | `20261117-145451-0ab458a4` |
| `UOpenGolf.Range.CustomShot` | `20261117-145521-8b551c17` |
| `UOpenGolf.Range.ReplaySelection` | `20261117-145534-ba3d0c0a` |
| `UOpenGolf.Smoke` | `20261117-145550-0b5b3faf` |
| `UOpenGolf.Menu.HudNavigation`, `UOpenGolf.Range.WindControls`, `InputBindings` and `HudControls` | `20261117-1456*` |
| Rendered `UOpenGolf.Visual.CustomShotPanel` (new), `ReplaySelector` and `RangeHudControls` | `20261117-145701-3ce410d0`, `-145756-*`, `-145825-*` |

Six `test_check_package_start.py` tests passed. `ue.ps1 Package` passed its start check
(`20261117-150025-package-8823fe05`), and the manifest lists `UOpenGolf/UI/WBP_WindIndicator.uasset`. A headless
start of that package listed a live `WBP_WindIndicator_C` with its `WindIndicator`.

Not verified:

- the packaged wind HUD seen on screen;
- typing speeds in km/h or m/s;
- gamepad navigation into the fields;
- Linux.

## Turf landing read from the impact; club labels dropped — 2026-11-17 (Windows)

Superseded the same day: the fit values, lm results and baseline gate failures below were
replaced by "Review fixes: turf grip bound, ground wind, topped shots" above.

A review of the 2026-11-16 fixes against standard contact physics found the lm fit came mostly
from an inherited multiplier. On the 115 airborne lm shots, the first impact's loss of forward
speed was 43% the Coulomb friction impulse, 54% the retention multiplier
(`0.95 × clamp(1 − rpm/8000, 0.4, 1)`) and 3% the landing bite. The multiplier's ramp only patched
the ground-launch runaway the multiplier itself caused. The bite was chosen from the launch's spin
and angle, not from the impact.

- **Turf landing.** A turf impact now keeps the standard restitution and Coulomb-capped friction
  impulse, then `exp(−k·vn/vt) · max(0, 1 − c·R·ωb/vt)` of its tangential velocity and contact-plane
  spin (`LandingAngleLoss`, `LandingSpinLoss`). It is read from the impact alone. It replaces the
  first-impact retention, its ramp, the landing-bucket bite and the rollout-bounce retention on
  fairway, soft fairway, rough, firm and fringe. The fields added on 2026-11-16 for those layers are
  gone. Green, sand and `HistoricalReference()` are unchanged.
- **Fit.** `UOpenGolf.Calibration.Rollout` (opt-in) searches the new law with rolling friction and
  both grass drags. It keeps total distance continuous across launch angle, with a margin under the
  regression limits, and holds the fairway between 6 ft (a judgment) and the green on a stimpmeter.
  Fairway: k 0.84, c 0.14, rolling friction 0.0735, grass drag 0.0229 plus 0.0039 /m speed squared,
  6.0 ft stimp.
- **Against lm.** Airborne roll error is 1.66 yd (1.96 yd on 2026-11-16) and total error
  5.4 yd (6.0). The six shots launched below 0° stop 32–47 yd short. lm totals those at about
  1.2 yd per mph but its 0.6–1.6° shots at about 0.93 yd per mph, and no model whose total follows
  launch angle can match both. A fit within ±10 yd there would have cost 0.6 yd of airborne error
  and 8.7 yd of extra roll on low launches; the airborne fit was chosen. Against
  the baseline `pre_contact_20261116.csv`, `shot_compare.py --check-baseline`
  fails three gates through those rows: roll error 3.42 → 3.67 yd, worst roll 34.3 → 47.1 yd and
  ground class 22.2 → 29.4 yd. Total error and provenance pass. The gates were left as they are.
- **Club labels.** The lm export's club is the one selected in the app (club speed and smash
  factor export as 0); those shots were irons. `shots.csv` now names the column `reported_club`,
  and nothing reads it. `shot_compare.py` lost its per-club table and `--club`, and gained errors by
  ball speed. Tests and docs describe shots by launch values.
- **Also in this set** (made on 2026-11-16 after the entry below and kept):
  - air drag on a ball on the ground;
  - contact at t = 0 for a launch along or into the ground;
  - the stimp solve includes air drag;
  - a 120 s headless time limit;
  - `UOpenGolf.Range.RecordedContactParity`.

  Before this change that set failed the 15% launch-angle check (17%).

Validation: Unreal Engine 5.8, Windows x64, Development editor built. `UOpenGolf.Physics` passed 21
of 21 (`Saved/Automation/20261117-094954-f3bf7f6c`); its one warning is `ShotInterrupt`'s intended
stop. `UOpenGolf.Range` passed 35 of 35, four with the usual warnings
(`Saved/Automation/20261117-095006-a7bfb3a3`). In that run the four recorded problem launches rest
within 0.3 yd of flat fairway: 48.5, 45.3, 84.5 and 110.9 yd. The drive totals 264.3 yd.
`UOpenGolf.Calibration.Rollout` ran in `Saved/Automation/20261117-095234-8f56336f`. All 23 Python
tests for the three scripts passed.

Not verified:

- flight: carry runs 2–4 yd short below 90 mph and 4 yd long above 105 mph;
- shots above 121 mph ball speed;
- rough and the other turf surfaces;
- Linux;
- packaged builds;
- a real Square Home shot in play.

## Turf rollout recalibrated to launch-monitor shots — 2026-11-16 (Windows)

Recorded range shots showed ground-launched shots running 129–263 yd, a tilted-axis shot (107.8 mph,
85° spin axis) checked by its sidespin, and full shots stopping dead on fairway. The labels below
(GW #1, 7i #4 and so on) are the clubs selected when the shots were recorded, which were not reliable. Impacts and ground contact now
read axis-aware spin, and the five turf surfaces play a catalog fitted to a 124-shot launch-monitor
export (lm, now in the local reference data). The fit added a speed-squared grass drag, cut
the first-impact loss and landing bite, and took the impact-spin friction multiplier off turf.
`HistoricalReference()` keeps the ported model. lm roll error fell from 22.9 to 3.4 yd on
average. The ground step is now coupled (`FRolloutProfile::bCoupledContact`): drag and rolling
resistance, then a friction impulse that leaves each step rolling, instead of letting a rolling
ball slip until a threshold. It leaves the lm fit unchanged (3.42 yd either way). The ball
actor and the headless simulator gained an optional diagnostic trace, and the simulator a
timestep and surface override for experiments. The earlier shot library was not restored;
`Assets/Scripts/shot_compare.py`, replacing the earlier comparison script, reads the reference data, writes a
provenance manifest and can gate a run against a baseline (`--check-baseline`). The new
export importer (now `monitor_import.py`) adds launch-monitor exports, and `recording_import.py` added the 36 recorded launches
(`recorded_20261113`, replayed but not compared). See the
[physics reference](../Plugins/OpenFairwayPhysics/Docs/Reference.md#turf-calibration).

`UOpenGolf.Range.Shot` compared a drive's roll-out on the range's real terrain against flat
headless ground with a 1 yd offline margin, so the longer roll took it to 1.21 yd. Its new trace
shows the live and headless balls landing at the same offline point (-9.02 yd each); the
whole gap appears during roll-out. There the live ball rolls 6.6 s over fairway falling 0.66° to
the right, ending 1.2 yd right of the flat prediction. Before the recalibration the same slope
gave 0.80 yd over a 14 yd roll. On flat ground the live ball matches the prediction within 0.1 yd
(`UOpenGolf.Range.RolloutSurfaces`). The test now requires landing offline within 0.5 yd, rest
offline within 2 yd (as for total), and any roll-out difference to point downhill.

Validation: Unreal Engine 5.8, Windows x64, Development editor built. `UOpenGolf.Physics` passed
21 of 21 (`Saved/Automation/20261116-164024-0e954557`), including `RolloutRegression`,
`ShotLibrary` and the historical `Fixtures`; the one warning is `ShotInterrupt`'s intended safety
stop. `UOpenGolf.Range` passed 34 of 34, four with warnings as in earlier runs, including
`Range.Shot` after the change above and the new `RecordedShots`
(`Saved/Automation/20261116-164211-4aae2f48`). `shot_compare.py` replayed all 160 library shots and
reported the 124 lm shots (`Saved/ShotLibrary/history/20261116-163329.csv`). All 19 Python tests
for the three scripts passed.

The new `UOpenGolf.Range.RecordedShots` hits the recorded problem shots on the range
(`Saved/Automation/20261116-164143-4e7a82dd`). GW #1 now totals 70.5 yd (recorded 129.2), GW #4
64.6 (133.4), 9i #3 109.0 (262.7) and 7i #4 121.4 (75.4). Each rests on fairway within 1.4 yd of
its flat-fairway prediction.

Not verified: `--check-baseline`, which needs a saved baseline run; drivers, rough and the other
turf surfaces, which have no reference shots; Linux; packaged builds; a real Square Home shot in
play after the change.

## Sign-in: standing QR code, two buttons, switching users — 2026-11-13 (Windows)

Signed out, every Settings page shows a QR code under the last category, captioned
**Sign-In** with the short code to compare on the phone. It needs no button and is removed
at sign-in. Settings › Player now offers only **Sign In** and **Create Account**, side by
side; both use this computer's browser. **Sign in with QR**, **Sign in on this computer**
and **Open verification page** are gone.

- Plugin: `SignInWithQr()` became `KeepQrSignInReady()`, a lease the new `SOpenGolfQrSignIn`
  widget renews each tick. The service requests a device code, polls it, and replaces one
  that expired or was declined, only while the widget is on screen. Holding a code no longer
  counts as signing in, and cancelling a browser sign-in keeps the code.
- Switching users: browser sign-in always sends `prompt=login`. Before, a browser still
  logged in to Auth0 approved the sign-in silently for its last user, so a player who signed
  out was signed back in as the same account.
- The QR square is 70% of the category column's width. The code fills it inside a
  two-module white border, with module edges on whole screen pixels. The account block's
  reserved height dropped from 230 to 170 now that it holds no QR code.
- Settings: **Camera & HUD** is now **Camera**, and **HUD scale** moved to **Accessibility**,
  under **Text size**.

Validation: Unreal Engine 5.8.4, Windows x64, Development editor built. `OpenGolf.Online`
passed 6 of 6 (`Saved/Automation/20261113-001010-5bde5762`), with new checks for the lease,
expiry, decline, cancel and sign-out. The rendered `UOpenGolf.Visual.Online` passed
(`Saved/Automation/20261113-000914-b40664d5`); its screenshots
(`Saved/Screenshots/Online/7252EA43436F82B7D741E687BD69DDAF/`) show the QR code under
**Data & system** while signed out and no QR code while signed in. The full `UOpenGolf` run
passed 102 of 103 (`Saved/Automation/20261113-001023-b5e875e7`); the failure,
`UOpenGolf.Physics.ShotLibrary`, is the working tree's deletion of `Data/ShotLibrary`. That
run also compiled and covered the Data & system edits of 2026-11-12 (Browse under the path
field, no duplicate path, **Updates** without help text).

Not verified: a real Auth0 sign-in, by browser or by phone, including that `prompt=login`
lets a second account sign in; the QR code scanned from a real screen.

## Windows installer and in-game updates — 2026-11-12 (Windows)

New `OpenGameUpdate` plugin: checks the latest published GitHub release, downloads its Setup
with progress, verifies its SHA-256 and starts it silently on the game's folder; the game then
quits and Setup restarts it. The plugin has no UI or game code. Its network side is the
`IGameUpdateSource` interface (GitHub built in), so tests and other hosts can swap it. The game
adds an update screen with a progress bar, an **Update to v…** button on the home and pause
pages, a version notice, and **Settings › Data & system › Updates**. The Package Windows
workflow now also builds `UOpenGolf-<version>-Windows-x64-Setup.exe` (Inno Setup 6,
`Assets/Installer/`) with its `.sha256`. The same page gained **Browse** for both folders
(Windows) and a shorter Shot recording note.

Validation: Unreal Engine 5.8.4, Windows x64. Development editor and game targets built.
`OpenGameUpdate` passed 6 of 6 (`Saved/Automation/20261112-231907-20a2c046`); `UOpenGolf.Menu`
passed 9 of 9. `OpenGameUpdate.Live` read the real v0.0.7 release through GitHub's download
redirect, and its parsed checksum matched the published `.sha256`. The full `UOpenGolf` run
passed 87 of 88 (`Saved/Automation/20261112-231516-a4be4317`); the failure,
`UOpenGolf.Physics.ShotLibrary`, is the working tree's deletion of `Data/ShotLibrary`, not this
change. Release-script tests passed (34). With a stand-in package, Setup installed silently,
waited for a locked game executable during an upgrade, replaced content, kept `Saved\`, and
uninstalled cleanly.

Not yet verified: a full in-game update, which needs the first published release with a Setup
asset; the workflow's installer step on the runner; and the folder picker by hand.

## Repository cleanup — 2026-11-12 (Windows)

Fixed the savanna scripts' imports after the move to `Assets/Scripts`, and made
asset pruning scan that directory. A regression test protects assets named in
project and plugin sources. Public game-module dependencies now match the types
included by its public headers. Removed the unused native tree-count method and
the retired tracer benchmark implementation and switch.

Unreal's Asset Registry confirmed three unreferenced vegetation runs: 72 packages
under `Terrain/Generated/Vegetation_DC91AC…`, one under
`Terrain/Savanna/CutTurf/Vegetation_C8FD58…`, and 32 under
`Terrain/Savanna/CutTurf/Vegetation_C93E30…`. Removed all 105 packages (15,235,415 bytes,
14.53 MiB) after backing them up to
`Saved/AssetBackups/20261113-053216-generated-cleanup/`. The five ground meshes in
`Build_F1319D…` are referenced by the saved map and were retained. The map was not
resaved. This reduces checkout content; existing Git LFS history remains.

Audit evidence: `Saved/Cleanup/generated-assets-audit.json` and
`Saved/Cleanup/PruneGeneratedAssets.log`. Unreal deleted 71 packages, then reported
that the first directory could not be removed. With the editor stopped, the remaining
34 audited files were checked byte-for-byte against their backups and removed.

Validation: Unreal Engine 5.8.4, changelist 58902052, Windows x64. Development editor
build passed (`Saved/Logs/BuildEditor.log`). All 245 engine-free Python tests passed
with Python 3.14, Pillow 13.0.0 and NumPy 2.5.4 (82 project utilities, 58 course,
61 savanna and 44 Blender helpers). Fixed three Windows portability failures in
the existing tests. All tracked Markdown links to local files and heading anchors
resolve; missing references now point to maintained plugin documentation.

After content removal, `Saved/Automation/20261112-213323-af5729ef/index.json`
recorded 31 of 32 range tests passing, including saved layout, presentation, rollout
surfaces, frame-rate/hitch equivalence and shot-trail history. `WindControls` had two
stale widget-type expectations: the indicator is now a focusable `SButton`, rather
than its earlier `SBorder`. Corrected those assertions. The startup smoke test passed
in `Saved/Automation/20261112-213637-ee7ca364/index.json`.
After a successful editor rebuild, the focused `WindControls` rerun passed in
`Saved/Automation/20261112-213753-e014f6e0/index.json`, completing coverage of all
32 range checks across the full run and focused rerun. Linux, rendered gameplay
and packaged builds were not revalidated in this cleanup.

Follow-up build check: the ignored local `.vscode/tasks.json` still invoked
`Scripts/ue.ps1` in four tasks. Updated those paths to `Assets/Scripts/ue.ps1` and
executed the default VS Code editor-build task successfully. The standalone
Windows Development game target also built successfully (`Saved/Logs/BuildGame.log`).
The development guide now explains how to repair older local task configurations.

## Historical VS Code setup evidence

Linux, 2026-10-16: `projectfiles` and `compilecommands` succeeded with a writable
engine directory. The generated workspace supplied `cppdbg`/gdb launch entries.

An earlier Windows setup attempt (date not recorded) could not write
`Engine/Intermediate/ProjectFiles/PrimaryProjectPath.txt` in the protected engine
installation. Compilation database generation succeeded separately, and its
source/compiler/response-file paths were checked. Interactive VS Code IntelliSense,
debugging and full workspace generation were not verified in that attempt.
Machine-specific starter tasks and launch settings from that checkout were ignored
local files and are not supplied by a fresh clone.

## Shared online accounts — 2026-11-11

Implementation is available in the reusable `OpenGolfOnline` plugin, with a
UOpenGolf adapter for measured range shots. The plugin owns QR/browser sign-in,
secure session storage, refresh/logout, player API access, profile/history UI,
and the account-bound upload queue. Public host configuration and club artwork
remain in the game. No server deployment or database migration was performed.

### Build and tests

Engine: Unreal Engine 5.8.4, changelist 58902052. Platform: Linux x86-64.
Both the Development editor and standalone Development game targets built.
The Linux Development package also built, and its 60-second headless startup
check passed (`Saved/Automation/20261111-181349-package-ArKxo7/Game.log`). All four
club PNGs were staged and matched the source files byte for byte.
Windows code is implemented but has not been compiled or exercised here.

`Saved/Automation/Online-Verified/index.json` contains nine successful checks:

- Browser callback state/path/method checks, including duplicate-state rejection.
- An isolated OS credential save/read/delete test using a generated value.
- Live Auth0 device authorization over verified TLS: HTTP 200, without completing
  a player login or recording authorization codes in the report.
- Session refresh/logout/account switching and pending-queue restart recovery.
- Real-shot eligibility, late club readings and stable retry/session identity.
- API shot fields, rounding, null measurements and exact decimal JSON output.
- Request-origin, method, query and header-injection boundaries.
- Rendered main/pause/settings menus.
- Rendered QR, profile and per-club shot-history screens using isolated fake data.

The final QR screenshot was decoded with OpenCV and matched the expected test
verification URL. Screenshots are under
`Saved/Screenshots/Online/01A1CD81DB627884B804B4EBD8C4F2F3/`.

The preceding `Online-UI-Final` report also records sixteen successful existing
menu/range/recording regressions, including shot conversion, auto reset, HUD
controls, replay and recording recovery. Its two visual-test failures were
corrected in the test drivers and rerun successfully in `Online-Verified`:
Escape was being intercepted by the editor rather than the game, and the account
test needed to wait for Slate to display the completed login state. The later
JSON precision fix is covered by the final contract and restart tests, not by
those earlier regression results.

The Linux keyring library is kept loaded for the process because GLib retains
its registered type names. This fixes the crash found when restarting Play.
The corrected keyring test and repeated Play sessions in the rendered menu run
completed successfully.

Only Unreal build-rule C# files changed. The requested `OpenFairway.sln` command
was attempted and failed because that solution is absent. Its formatting check
is unavailable; UnrealBuildTool evaluated both updated module rules successfully.

### Rendered stability

Only two-dimensional club PNGs were added; terrain, turf, vegetation, lighting
and three-dimensional materials were unchanged. The public-map Play capture
`UOpenGolf.Visual.CoastalTerrain` completed 200 frames in
`Saved/Automation/CoastalReview/OnlinePlugin-20261112-012347/`.
The editor crashed afterward on the next Play startup due to the subsequently
fixed keyring lifetime issue; this capture was not a clean whole-process test run.

One editor rendered the sequences. Viewport size was 1541 × 953, with the same
camera and AA configuration for grass-on/off views. The following results came
from `Assets/Scripts/grass_stability.py --profile coastal`; changed pixels use a
3/255 threshold and are averaged across consecutive frames.

| View | Lower-region changed | Worst horizontal band changed |
| --- | ---: | ---: |
| Tee, grass on | 0.001% | 2.033% |
| Tee, grass off | 0.000% | 2.083% |
| Grass edge, on | 0.001% | 1.881% |
| Grass edge, off | 0.001% | 1.988% |
| Close grass, on | 0.017% | 0.762% |
| Close grass, off | 0.001% | 1.660% |

Canopy sparkle above 16/255 was 0.014% at the tee, 0.025% at the green,
0.026% at the edge and 0.010% at the bunker. A matched before capture for the new
UI artwork is unavailable, so these are measurements, not a before/after
improvement claim. The slow-camera sequence was captured; detailed motion review
remains pending. No terrain/texture changes followed this capture. Subsequent
changes concerned authentication, serialization, module dependencies and tests.

### Acceptance limits

A player has not yet approved a real login in this implementation. Confirm the
same account's existing profile/history in both games, then save a real measured
range shot in UOpenGolf and check it in OGolf. This is the remaining live
cross-game/PostgreSQL acceptance check; fake visual data is not evidence of it.
Also verify browser/social login, refresh across an application restart, and
Windows credential storage in a Windows build. A full packaged-game login has not been approved. No rendered FPS claim is made.

## Account access controls — 2026-11-11

The home/pause header now has an account button at its far right, on the same row
as UOPEN GOLF. It draws a login icon while signed out and a cog while signed in.
The cog opens user settings directly. The logout icon appears inside those
settings. Signed-in views no longer expose sign-in, account creation or the
removed "Try again" action. The old full-width account menu entry was removed.
The controls are drawn in Slate and have tooltips and accessible names.

User settings load club statistics after the first profile sync is ready. Opening
the active cog again preserves the current settings view and its edited name.

Development and DebugGame editor builds passed on Linux with Unreal 5.8.4.
`Saved/Automation/Account-Header-Verified/index.json` records four successful
checks: session behavior, menu input isolation, rendered menus, and the online
screen. The online screen check covers the login-to-cog change, absence of login
choices while signed in, logout through its icon, restoration of signed-out
choices, and waiting for a delayed first profile sync before loading statistics.
The rendered QR still decodes to the expected test verification URL. These tests
use an isolated account fixture and do not establish a real account login.

The Linux Development package was rebuilt. Its 60-second startup check passed
in `Saved/Automation/20261111-185637-package-5WJKHL/Game.log`. The rendered checks
finished before a separate DebugGame editor was observed; that editor subsequently
exited without being closed by the agent. No FPS comparison was made.

Screenshots are under
`Saved/Screenshots/Online/01A1CDA2E2F77F69A33D84BBF3D22D29/`.
No texture, terrain, lighting or vegetation assets were changed for these drawn
controls. No C# source changed in this UI revision.

## Menu and account page look — 2026-11-11

The main menu and the account page now share one look. Buttons are rounded and
outlined in the HUD colors, with the light accent for primary actions. Settings and
User settings use the same title row: a drawn back-arrow icon button, a 32-point
title and the page's right-hand control. The back arrow, gear, sign-in, sign-out
and delete icons are all drawn by one plugin widget. The account page groups its
controls into Profile, Range shots and Club averages sections. Club rows show
columns and are tagged by club. In shot history, the page back control and Escape
return to the club list before leaving the page.

The OpenGolfOnline plugin stays independent of the game. The game passes its
style through a new `FOpenGolfAccountStyle`, and other hosts keep the plugin
default. Fonts follow the Text size preference.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive
unity disabled and with every changed file compiled on its own.
`Saved/Automation/20261111-202035-MenuRestyle/index.json` records
`UOpenGolf.Visual.Menu` and `UOpenGolf.Visual.Online` as successful. No other
editor was running. The online test now also covers back from shot history to the
club list and reopening a club. Screenshots are under
`Saved/Screenshots/Menu/20261111-2021*` and
`Saved/Screenshots/Online/01A1CDFD241275FCA94822586AD48149/`.

Limits: the online test renders the plugin panel with the game style and its own
title row. The game's account page, which uses the menu's page header, was not
captured. The DebugGame editor, packaged game and Windows build were not rebuilt
or checked. The owner has not yet reviewed the look.

## Range HUD status icons and recording — 2026-11-11

The range HUD's club picker could not be clicked: the whole shot HUD ignored the
mouse. Its root now passes clicks through, as the play buttons and wind controls do,
and only the club picker and the status box's two buttons take clicks.

The status box shows **Aim**, then two drawn icons. The launch-monitor icon is colored
by connection state and shows the battery level when known; it is captioned
**Connect [C]** while disconnected and opens Settings › Simulator when clicked. The
record toggle (**Record [F9]**) is a red dot with the saved count while recording and a
gray ring when off; clicking it does what **F9** does. Failed saves turn it amber, and
errors show beside the icons. The Launch Monitor and Battery text cells and the key-tip
line were removed.

Recording now decides online saving as well: while recording and signed in, eligible
range shots are queued for the online profile as well as written locally. The **Save to
profile** checkboxes (HUD and User settings) and the plugin's `save_to_profile` setting
were removed; an older player file's value is ignored.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive unity
disabled and with every changed file compiled on its own. A new rendered test,
`UOpenGolf.Visual.RangeHudControls`, checks that clicks reach the club picker and both
status buttons, that the record button toggles recording, and that the launch-monitor
button opens Settings › Simulator. **It has not been run yet**, nor have the updated
`UOpenGolf.Range.ShotDisplay` and `UOpenGolf.Range.ShotRecording` checks: the owner's
DebugGame editor was open, and a second editor process was not started.

Not verified: a real launch monitor's battery and Ready state in the icons, and a real
recorded shot reaching the online profile. The DebugGame editor, packaged game and
Windows build were not rebuilt.

## Range HUD polish and no tooltips — 2026-11-11

The club picker shows standard abbreviations: DR, 3W–7W, 4i–9i, PW, GW, SW, LW and PT.
Recording file names and online club keys are unchanged.

The shot column used to resize when the club changed. Its width was the widest of the
heading row and the tile grid, and "Pitching Wedge" made the heading row wider. The grid
then stretched every tile. The column is now fixed at the grid's width, the picker has a
fixed size, and a long heading is shortened with an ellipsis.

"Waiting for a shot" was removed. While the ball waits at address, a soft glow hugs its
outline: red until the launch monitor reports the ball ready, green once it does. The owner
chose the outline-glow look. The HUD draws it over the ball's on-screen outline in Slate,
not as a post-process material, so no material asset, custom-depth setting or extra GPU
pass was added. It draws over anything in front of the ball.

While a ball is being placed, and for the rest of a hole with a placed ball, the hole
heading reads **Practice · Ball placement**. The Stroke, To Pin and Putts tiles and the
ball glow are hidden then too.

Recording is range-only. On the hole the record icon is hidden, **F9** is ignored and no
shot is written, even if recording was left on; the on/off state is kept for the range.
Cloud saving was already range-only.

Every tooltip was removed. This covers the HUD status icons, the menu's launch-monitor
button, the page back arrow, and the plugin's account, back, view-shots and delete
controls. Plugin icon buttons take a `Label` as their accessible name instead. The
rendered menu, online and HUD tests now fail if any widget in the game window or its
menus has a tooltip.

Development editor builds passed on Linux with Unreal 5.8.4, both with adaptive unity
disabled and with every changed file compiled on its own. No other editor was running
during the test runs below.

- `Saved/Automation/20261111-213049-HudPolishHeadless/index.json`: all 8 tests passed,
  covering ShotDisplay, ShotRecording and Online. The two warnings are the expected logs
  from the recording failure-injection tests.
- `Saved/Automation/20261111-213114-HudPolishRendered/index.json`: RangeHudControls,
  Menu and Online all failed. The causes were two test problems, now corrected:
  - The tooltip sweep counted the empty tooltips engine widgets carry: SComboButton's
    inner button, and a collapsed SSearchBox throbber. Slate never shows those
    (`SlateUser.cpp:1383`). The sweep now counts only non-empty tooltips on visible
    widgets.
  - The record button was clicked in the frame the HUD reappeared, so the first click
    missed and recording toggled one click late. The test now waits for layout.

  All other checks in that run passed:
  - the same heading and tile sizes with DR and PW;
  - the glow at address on the range and on a normal hole, and none while placing or on
    a practice hole;
  - the placement and practice headings, with the hole tiles hidden;
  - no record toggle on the hole;
  - the launch-monitor button opening Settings › Simulator.

  Screenshots are in `Saved/Screenshots/RangeHud/20261111-2132*`.
- The glow showed red and green as intended. At 1280 × 720 the ball is about 6 px across
  and the glow was hard to see, so its strokes were widened slightly after this run.
- Rerun with the corrected tests and the widened glow, with no other editor running:
  `Saved/Automation/20261111-220240-SettingsRendered/index.json`. RangeHudControls passed;
  its one warning is the engine's own analytics request to Epic failing without network.
  `Saved/Screenshots/RangeHud/20261111-220349-ball-not-ready.png` and
  `20261111-220351-ball-ready.png` show the wider red and green glow, clearly visible around
  the 6 px ball. The menu and online tests in that run cover the next section's changes.

Not verified: a real launch monitor turning the glow green, and the glow's size and
subtlety in real captures. The DebugGame editor, packaged game and Windows build were not
rebuilt.

## One Settings menu; the account in Settings › Player — 2026-11-11

Owner feedback merged the two settings screens. The cog was a signed-in-only **User
settings** page beside a separate **Settings** button; the logout icon only appeared on that
page, under the cog.

- **Top bar** on every menu page: **UOPEN GOLF**, the launch-monitor status, one account
  icon, then the cog. The account icon is the login icon while signed out (it opens
  Settings › Player) and the logout icon while signed in. The cog is always shown and is
  the only way into Settings; the home and pause pages lost their **Settings** button.
- **Settings › Player** holds the account. Signed out: the sign-in choices only. Signed in:
  member since, the display name and a **Shot history** row whose **View** opens club
  averages, then one club's shots; Back returns to Player. Settings rebuilds when the
  session starts or ends.
- **Time zone** (Player) sets shot-history times and the member-since date: Local (the
  default, this computer's zone), UTC or 16 listed zones such as Pacific (PT). Conversion
  uses ICU (`FText::AsDateTime` with an IANA ID), which applies daylight saving; the English
  ICU data set packaged with the game includes the zone data. It applies at once like the
  connection choices, and Reset All leaves it alone. CSV export keeps UTC.
- Clubs in averages and the history title use the standard abbreviations (7i, PW, DR).
- The **Range shots** pending/rejected counts were removed.
- Plugin: `SOpenGolfAccountButton` lost its cog state. Its sign-in and logout variants each
  show only in their session state (tags `AccountSignIn` and `AccountSignOut`).
  `SOpenGolfAccountPanel` gained `Page` (Account or Shots). The later local-time cleanup removed the zone override.

Development editor builds passed on Linux with Unreal 5.8.4, with adaptive unity disabled
and with `ue.sh build`. No other editor was running during these runs.

- `Saved/Automation/20261111-220212-SettingsHeadless/index.json`: all 19 tests passed,
  covering Menu, ShotDisplay, ShotRecording, the game's Online tests and the plugin's
  `OpenGolf.Online` tests. The new `OpenGolf.Online.Format` covers club abbreviations and
  Pacific times in winter (UTC−8) and summer (UTC−7). `UOpenGolf.Menu.PreferenceValidation`
  checks the time-zone default, recovery, disk round trip, and that each listed zone
  converts a July instant away from UTC, which catches a mistyped zone ID.
- The zone labels were then shortened, because the drop-down clipped "Local (this
  computer)". `Saved/Automation/20261111-220627-SettingsHeadless2` (Menu and Format, 7 tests)
  and `20261111-220644-SettingsRendered2` (Visual.Online and Visual.Menu) passed after that
  change.
- `UOpenGolf.Visual.Online` now drives a real menu against a stubbed service. It checks:
  - the top-bar order and which account icon shows;
  - that sign-in opens Player, with no profile options before sign-in;
  - the profile and Shot history after sign-in, with no range-shot counts;
  - choosing Pacific from the drop-down, the 7i club row, and the "Time (PT)" column
    showing the stub's 00:00 UTC shot as 4:00 PM on Nov 10;
  - back to the club list, then to Player;
  - logout from the top bar.

  `UOpenGolf.Visual.Menu` checks that the home and pause pages have no Settings button,
  the icon order, and the cog opening Settings. Captures are in
  `Saved/Screenshots/Online/01A1CE5E58297D32B8D9285BE0239D32/` and
  `Saved/Screenshots/Menu/20261111-2207*`.

Not verified: a real Auth0 sign-in and real shot data; time zones on Windows (ICU maps the
Windows zone for Local); the DebugGame editor, packaged game and Windows build.

### DebugGame crash on Play, fixed — 2026-11-11

The owner's DebugGame editor crashed starting Play: a segmentation fault in `FMemory::Memcpy`
(`GenericPlatformMemory.h:654`). Reproduced under gdb with `UnrealEditor-Linux-DebugGame`
(`UOpenGolf.Visual.Menu`). The menu's account button called `IsConfigured()` on an object that
was not the online service, and copying its "configuration" string read from address 0.

Cause: the menu's new `Online` argument. `SLATE_ARGUMENT` declares its member without an
initializer, and the player controller creates the menu without passing `Online`, so it held
leftover memory. In Development builds that memory happened to be null, so the earlier test
runs passed; in DebugGame it resolved to an unrelated object.

Fix: every raw-pointer Slate argument now defaults to `nullptr`. That covers `SMainMenu`'s
`Controller` and `Online`, `SClubPicker`'s `Monitor`, and `Online` on the plugin's account
button and panel. The new headless test `UOpenGolf.UI.ArgumentDefaults` builds each widget's
arguments in pattern-filled memory and checks those defaults. It failed with the menu's
defaults removed (`Saved/Automation/20261111-222634-ArgDefaultsRed`) and passes with them.

The Development (adaptive and non-adaptive) and DebugGame editor builds passed. Under gdb, the
DebugGame editor ran ArgumentDefaults, Visual.Menu, Visual.Online and Visual.RangeHudControls
with no crash, and all passed (`Saved/Automation/20261111-222808-DebugGameFixed`). The one
warning is the engine's analytics request to Epic failing without network.

## UI folders, cleanup pass and sensitive-data scan — 2026-11-12

After the DebugGame fix, HUD keyboard/gamepad navigation, a routed menu with cached pages, and
account states with a sign-out restore marker were added (2026-11-11, 22:42–22:56). Their last
headless run (`Saved/Automation/UiPolishHeadless`) had one failure and predated the final menu
edit. This pass reviewed the working tree.

- **Layout.** `Source/UOpenGolf/Private/UI` moved into `Common`, `Hud`, `Club`, `Wind` and
  `Menu` (45 files, includes rewritten in 47). See [Architecture](Architecture.md).
- **`UOpenGolf.Menu.ReturnToMainMenu`** failed in that earlier run; it passes on the current
  code, in Development and DebugGame.
- **Fixed.**
  - Shot history reopened on the last club's cached page; each View now builds a fresh page.
  - The account panel's own heading (hosts using `ShowHeading`) never updated; it now follows
    the page and session.
  - The Shots panel requested club averages twice on every visit. The new
    `OpenGolf.Online.PanelHeading` test found this.
- **Removed.** An unused gamepad flag, an unused panel revision field, and duplicate
  visibility code in Settings › Player.
- **Tests corrected, not game code.**
  - `Menu.RenderResolution` read this machine's saved video settings.
  - `Visual.WindIndicator` expected the HUD root to be hit-test invisible; it has been
    self-hit-test-invisible since its icons became clickable.
  - `Visual.RangeAssets` set the grass variable at a priority below the coastal test's, so it
    failed only when the whole Visual group ran in one process.

Builds passed: Development (adaptive unity off, and `ue.sh build`) and DebugGame. No other
editor ran during these runs.

- Headless, `Saved/Automation/20261112-181630-CleanupHeadless`: 23 of 23 passed (Menu, UI,
  Online, ShotDisplay, ShotRecording, InputBindings and the plugin's `OpenGolf.Online`). The
  two warnings are the recording tests' expected failure-injection logs.
- Rendered, all eleven `UOpenGolf.Visual` tests, `20261112-181701-CleanupRendered`: nine
  passed; RangeAssets and WindIndicator failed for the test reasons above. Rerun with
  CoastalTerrain after the corrections, `20261112-182445-CleanupRendered2`: all three passed.
- DebugGame under gdb, `20261112-182934-CleanupDebugGame`: ReturnToMainMenu, ArgumentDefaults,
  PanelHeading, Visual.Menu, Visual.Online and Visual.RangeHudControls passed with no signal.
  Captures: `Saved/Screenshots/Online/01A1D2BE401D7B88A55498AE1931896F/`.

**Sensitive-data scan** of 457 text files, 424 assets and all history (the repository is public):

- No passwords, tokens, private keys or `.env` files. The Auth0 client ID is a public
  native-application identifier.
- `Content/Developers/<account>/` is created by the editor. It was empty and never committed;
  it is now ignored.
- `Config/DefaultEditorPerProjectUserSettings.ini` carried a home-folder path in its terminal
  startup commands. Those lines moved to the ignored per-user layer. Earlier commits keep them.
- 185 assets embed their import source paths, which include a home folder. Left as they are by
  the owner's choice, as is the commit history's author emails.

Not verified: a real Auth0 sign-in and real shot data; a real gamepad (navigation is covered by
simulated key events only); Windows and packaged builds.
