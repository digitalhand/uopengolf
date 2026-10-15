# Phase status

Current status and completion evidence for each phase of the
[roadmap](Roadmap.md). The roadmap defines each phase's scope and exit criteria;
this document records where each phase stands. Update it as evidence changes,
including the engine version, OS and date actually tested.

Phases 1 and 2 are split into a **Windows track** (current) and a **Linux track**
(deferred, not blocking Windows work). Each phase's exit still requires both tracks.

| Phase | Windows track | Linux track |
| --- | --- | --- |
| 0 — UOpenGolf foundation | Complete | Commands provided; not run |
| 1 — Linux baseline and integration design | Launch-monitor design and physics fixtures done | Not started |
| 2 — Launch-monitor input | Square Home verified on hardware; lifecycle checks remain | Not started (no BlueZ transport) |
| 3 — Native OpenFairway | Core, simulator and ball actor ported; fixtures pass | Not started |
| 4 — Greens and surfaces | Not started | Not started |
| 5 — End-to-end golf slice | Started: whitebox range hits device and replay shots | Not started |
| 6 — Community and content expansion | Not started | Not started |

## Phase 0 — UOpenGolf foundation

**Status:** complete on Windows. The editor target builds and
`UOpenGolf.Smoke.UOpenGolf` passes headless with Unreal 5.8.3 (2026-10-14), after
aligning the HUD greeting with the test. The maintainer opened and compiled the
project in the editor; the rendered greeting should still be checked visually.
Linux execution remains unverified.

## Phase 1 — Linux baseline and integration design

**Status:** Windows track done for the launch monitor: see
[launch-monitor integration](LaunchMonitorIntegration.md) (pinned revision, native
C++ port decision, units, signs, missing values, threading, ownership, lifecycle).
The physics baseline is done on Windows: 21 golden fixtures from the ogolf-physics C#
sources at `e6d793a` (seven surfaces, a putt and a spin-back) with tolerances of 0.1 yd
for distances, 0.1 ft for apex and 0.01 s for hang time; the fixed step is 120 Hz. See
the [OpenFairway Physics README](../Plugins/OpenFairwayPhysics/README.md). The Linux
baseline has not started.

## Phase 2 — Launch-monitor input

**Status:** Windows implementation working with a real Square Home. The
`OpenLaunchMonitor` plugin ports the Square protocol/session to C++, adds a C++/WinRT
Bluetooth LE transport, replay input, Blueprint events, a HUD status panel, and
hardware-free protocol, session and replay tests (all passing, 2026-10-14).

On 2026-10-14 the maintainer connected a real Square Home on Windows: the connection,
battery and firmware readout, shot data and club data all worked. Still to verify on
hardware: reconnect after link loss and club selection. The firmware version and
Windows build tested have not been recorded yet. Linux BlueZ is not implemented.

Shot recording (2026-10-15): with a launch monitor connected, **F9** saves each real
device shot as GSPro-shaped JSON with the selected club, club data and the engine result
under `Documents/UOpenGolf/Saved/ShotRecordings/session_<timestamp>/club_<club>_shot_<n>.json` (README
"Recording shots"). `UOpenGolf.Range.ShotRecording` passes headless
(`Saved/Automation/20261015-134115-0067205c`); not yet run with a real Square Home.

## Phase 3 — Native OpenFairway

**Status:** Windows implementation in place (2026-10-14). The `OpenFairwayPhysics` plugin
ports the ogolf-physics core equations, surfaces, ball profile, launch regimes and
headless shot loop to C++, with an Unreal ball actor that uses Chaos sweeps and traces
for contact (Chaos does not integrate the ball). Automation on Windows, Unreal 5.8.3:
`UOpenGolf.Physics.Core` and `UOpenGolf.Physics.Fixtures` pass, the fixtures matching the
C# reference to about 0.0001 yd; `UOpenGolf.Range.Shot` lands and stops a drive on the
running range within tolerance of the headless result.

Remaining for the exit: JSON ball profiles, slope and terrain contact coverage (the
range is flat), full ball/camera visual review, and the whole phase on Linux.

Additional analysis (Windows, Unreal 5.8.3, 2026-10-14): the
[FlightScope comparison](FlightScopeAnalysis.md) reproduces a 4.91 yd carry shortfall
and confirms discontinuities at calibration speed boundaries. Editor build, all
three physics tests (including the new diagnostic) and startup smoke pass. This
established reproducibility, not FlightScope accuracy. Follow-up corrections replace
constant spin decay with a speed-dependent model and interpolate calibration scales
and landing responses. Historical C# fixtures use an explicit reference profile.
The screenshot shot now carries 85.94 yd versus the original 82.69 yd and FlightScope's
87.6 yd; independent physical calibration remains outstanding. The actor also spends
the remaining landing timestep, correcting a rollout mismatch exposed by the new model.

Final validation of those corrections (Windows, Unreal 5.8.3, 2026-10-14): editor
build, all 11 headless `UOpenGolf` tests and a separate startup smoke pass. The
rendered `UOpenGolf.Visual.LaunchDirections` test passes; its left/up and right/down
HUD captures were visually inspected. Headless report:
`Saved/Automation/20261014-230126-b2e8eb64`; smoke:
`Saved/Automation/20261014-230219-11ff00ec`; rendered:
`Saved/Automation/LaunchDirectionsCached`. No Linux or new hardware validation.

FlightScope shot library (Windows, Unreal 5.8.3, 2026-10-14): `Data/ShotLibrary/` holds
the 222 recorded shots from openfairway's sessions (`4f3e11b`), 175 of them with
FlightScope results. `UOpenGolf.Physics.ShotLibrary` replays them on fairway at 75 °F,
and `python Scripts/flightscope_compare.py` reports engine minus FlightScope. First run
(`Saved/Automation/20261014-234524-c549296e`), averaged over the 175 shots:

| Profile | Carry bias / avg abs (yd) | Carry within 3 / 5 yd | Total bias / avg abs (yd) | Roll bias (yd) | Apex bias / avg abs (ft) | Pass / moderate / severe |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Current | +2.5 / 3.8 | 55% / 71% | −0.4 / 4.9 | −2.9 | −0.1 / 3.2 | 68 / 54 / 53 |
| Historical | +0.1 / 1.8 | 80% / 97% | +3.6 / 5.8 | +3.5 | −1.0 / 3.4 | 80 / 61 / 34 |

The current model carries longer than FlightScope, most for drivers (+4.8 yd average),
3-woods (+4.1) and 7-irons (+5.0), and rolls shorter. The historical profile matches
carry more closely but overshoots total. Its regime overrides were calibrated from
ogolf-physics FlightScope data that includes these sessions, so it is not an independent
check. All five `UOpenGolf.Physics` tests and the startup smoke pass
(`Saved/Automation/20261014-234602-50efffc4`, `Saved/Automation/20261014-234614-3123feb3`).
Linux is untested.

## Phase 4 — Greens and surfaces

**Status:** not started. Surface presets and tags exist; no ogolf code has been imported.

## Phase 5 — End-to-end golf slice

**Status:** started on Windows. Measured and replayed shots reach native physics on a
whitebox range with ball presentation, tracer, follow camera, shot information and
tee reset. Not yet: a real course, packaging automation, and Linux.

Follow-up (2026-10-14): nine named replay presets, including both supplied
FlightScope screenshots, are selectable through **Test shots [P]**; **R** repeats
the selection. The second shot exposed excess legacy low-launch lift. Gameplay
now disables that boost, giving 121.55 yd carry instead of 128.13 yd (FlightScope
119.8 yd); the earlier iron remains 85.94 yd. Regime boundaries and the decay law
are unchanged in this follow-up. The physics README includes full regime/bin
decoding, interpolation and landing-response explanations, and source references.

Windows editor build and all 12 headless project tests pass, including actual
range carry for the selected wood (`Saved/Automation/20261014-233128-032069a4`).
Rendered selector verification passes (`Saved/Automation/ReplaySelectorFinal`);
preview, expanded dropdown, and 121.5 yd HUD captures were visually inspected.
Separate startup smoke passes (`Saved/Automation/20261014-233523-38e15ca2`).
No Linux or new hardware testing.

HUD restyle (2026-10-15): the Canvas text panels are replaced by a Slate HUD
(`SRangeHud`). Translucent gray two-column tiles down the right edge show Total |
Carry first, then roll, apex, offline, hang time, landing angle/speed and club data.
The raw launch-monitor values are last, in darker tiles. A small bottom-right box
shows only the connection state, battery and a **Connect [C]** / **Disconnect [D]** tip.
Disconnect moved from **X** to **D**. The **Test shots [P] | Repeat [R]** button
shows only while no launch monitor is connected. Formatting is covered by
`UOpenGolf.Range.ShotDisplay`. Status: all changed files compile on Windows, but the
link step was blocked by an open editor. The build, headless tests and rendered
captures are not yet run. No physics change.

Rollout diagnosis (2026-10-14): `UOpenGolf.Range.RolloutSurfaces` reproduces the
low wood's mixed-range 31.92 yd roll / 153.47 yd total. Changing only surface tags
to fairway gives 37.69 / 159.24 yd with identical carry; keeping fringe but
replacing green gives 36.71 / 158.26 yd. The resolved regime contact multipliers
are neutral. No runtime physics or surface values changed in this investigation.
Windows editor build and all 14 current project tests pass; report:
`Saved/Automation/20261014-234231-61fb0e80`. Separate startup smoke passes:
`Saved/Automation/20261014-234420-030290be`. Linux remains untested.

Test-scene assignment (2026-10-14): all range ground meshes now have the Fairway
surface tag and appearance, including the former green/fringe area and side
strips. The ball's contact-based surface resolver and physics coefficients are
unchanged. The default low wood now ends on Fairway with 37.69 yd roll and
159.24 yd total. The surface regression verifies the default scene before
explicitly reconstructing the historical mixed assignments in its test world.
Windows editor build and all 14 project tests pass:
`Saved/Automation/20261014-234851-cf978ea3`. Separate smoke passes:
`Saved/Automation/20261014-235107-48ad60d0`. Rendered check passes:
`Saved/Automation/FairwayScene`; the scene and Fairway lie readout in
`Saved/Screenshots/ReplaySelector/20261014-235117-5.png` were visually inspected.
Linux remains untested.

Automatic tee reset (2026-10-15): three seconds after the ball stops, range mode
puts it back on the tee and the camera glides back. The shot result, launch-monitor
shot and tracer stay until the next shot. A shot hit during the wait cancels the
pending reset. `UOpenGolf.Range.AutoReset` verifies this: the low wood hit during
the previous shot's wait completes at 121.5 yd carry / 159.2 yd total, stays put
until the delay, then sits 0.00 cm from the tee with its result unchanged. On
Windows, the editor build, smoke (`Saved/Automation/20261015-002821-ae9a3809`), all
7 `UOpenGolf.Range` tests (`Saved/Automation/20261015-002837-403bdc2e`) and all 5
`UOpenGolf.Physics` tests (`Saved/Automation/20261015-003039-024c9808`) pass. No
physics code changed. The reset has not been checked in a rendered session, and
Linux remains untested.

Public range visuals and dark green palette (2026-10-15): the game now uses
CC0 Grass 004 textures, a dimpled golf ball with three LODs, and a target
flagstick. Original short grass uses non-colliding hierarchical instances near
the tee; `uopengolf.Grass3D 0` clears them and `1` recreates them. Fairway and
blade materials expose `GrassTint`, and the tee paint uses a matching darker
green. Asset provenance, checksums, conversion and regeneration are recorded in
[Range assets](RangeAssets.md) and the third-party notices. This presentation
change does not edit physics-plugin source or shot-library data.

Windows, Unreal 5.8.3: editor build and all 16 headless project tests pass,
including startup smoke, all five physics tests, and all seven range tests
(`Saved/Automation/20261015-005300-f9ffd98a`). The presentation test verifies
ball size and deterministic grass disable/re-enable behavior. The maintainer
confirmed visual verification in this session. Automated rendered captures and
grass frame-time measurements were not completed; Linux remains untested.

Explicit grass colors (2026-10-15): replaced the texture tint multiplier with
`GrassDarkColor` = sRGB `#406B1F` and `GrassLightColor` = sRGB `#4D7E20` on both
fairway and grass-blade materials. Mowing and texture variation blend between
these endpoints; the tee uses the dark color. The update utility converts sRGB
to linear RGB and verifies the saved parameter values. Editor build and all 16
headless project tests pass on Windows, Unreal 5.8.3:
`Saved/Automation/20261015-123610-1f202c67`. This exact palette has not had a new
rendered inspection; scene lighting and tone mapping affect displayed colors.
Linux remains untested.

## Phase 6 — Community and content expansion

**Status:** not started.
