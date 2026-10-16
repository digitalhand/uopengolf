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
hardware: reconnect after link loss. On 2026-10-15 the maintainer also verified club
selection and the club toggle on the real Square Home. The firmware version and
Windows build tested have not been recorded yet. Linux BlueZ is not implemented.

Shot recording (2026-10-15): with a launch monitor connected, **F9** saves each real
device shot as GSPro-shaped JSON with the selected club, club data and the engine result
under `Documents/UOpenGolf/Saved/ShotRecordings/session_<timestamp>/club_<club>_shot_<n>.json` (README
"Recording shots"). `UOpenGolf.Range.ShotRecording` passes headless
(`Saved/Automation/20261015-134115-0067205c`); the maintainer also validated recorded real Square Home shots on 2026-10-15.
Later reliability changes add a unique suffix to session folders, save retries,
late-club-data handling and Bluetooth cancellation. See the
[reliability validation](#recording-reliability-bluetooth-cancellation-and-tracer-cpu-work)
for current behavior, automated evidence and remaining hardware gaps.

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

Optional turf limited to the fairway (2026-10-15): a private range's ground
material override now applies only to fairway and fringe pieces
(`AUOpenGolfRange::TurfTag`), and 3D grass is clipped to the 40 m fairway strip
and the range start. Previously the override covered the 140 m-wide rough as well.
Collision and surface tags are unchanged. `UOpenGolf.Range` (8 tests) passes
(`Saved/Automation/20261015-155931-f69b247a`). The optional variant check passes
with a new "turf off rough" assertion
(`Saved/Automation/20261015-160556-variant-d730eb3d`), and its rendered captures
were inspected (`Saved/Automation/20261015-160646-variant-ade59700`). Windows only.
A per-machine `LocalRangeClass` setting in the ignored `Config/UserGame.ini` now
lets the default map use an installed private range. Automation tests ignore it.
In a `-game` run the log showed the private range being used, and a missing class
falls back to the public range with a warning. Smoke and `UOpenGolf.Range`
(`Saved/Automation/20261015-161611-5ed09967`) pass.

Range look pass (2026-10-15): the earlier renders were overexposed. Auto exposure is
off and no exposure was set, so the public fairway (`#406B1F` palette) rendered as
`#B2D594` and the private turf as `#D2E18D`. The reference photo measures `#80A151`.

Changes:
- The range now has an unbound post-process: -0.5 EV fixed exposure (`ExposureBiasEV`)
  and no motion blur.
- A collision-free 4 km backdrop plane hides the void past the range.
- `RoughMaterialOverride` covers the rough and the backdrop, and the green now takes the
  turf material.
- The private turf is palette-driven, with lengthwise 8 m stripes and anti-tiling.

The private tee view measures fairway `#7C9F4D` and rough `#739448`, against the photo's
`#80A151` and `#6E8E40`. Evidence:
- Smoke and `UOpenGolf.Range` pass (`Saved/Automation/20261015-165632-7e9796dc`).
- The optional variant passes with identical carry and total
  (`Saved/Automation/20261015-165804-variant-99440e8a`).
- Rendered captures of both maps were inspected. Windows only.

Range dimensions, calm turf and white tracer (2026-10-15): the game ball now
normalizes its imported bounds to 42.67 mm diameter. The original flag mesh is
213.36 cm above ground with a 1.27 cm pole and a 35.56 by 50.8 cm flag. Seven
feet is a conventional flagstick choice, not a mandated PGA height. The tee and
yardage markers now explicitly use the basic-shape color material instead of
falling back to the checkerboard material.

The game owns a feathered translucent white ribbon (2 cm width, 0.35 opacity).
It retains the whole flight path and fades monotonically during rollout below
5 m/s, disappearing at rest and clearing on reset or another shot. No physics
plugin source or shot-library data changed, so no FlightScope rerun was required.
The private/public comparison retained identical carry/total/rest positions:
iron 85.9318 / 89.0567 yd; low wood 121.5477 / 159.2398 yd.

Private turf uses 16 cm spacing and (0.45, 0.45, 0.10) scale, 38,471 instances.
Wind/player wobble and material displacement are disabled. The generated private
material chain retains vendor opacity shapes, adds distance fading, shares the
ground's mowing colors and uses upward canopy normals to suppress dark edge
shimmer. Ground detail repeats at 1 m with reduced normal/macro variation. Vendor
originals and the per-machine private startup selection are preserved.

Windows verification:
- Editor build passed. Smoke: `Saved/Automation/20261015-181940-93764773`.
- All nine range tests, including actual dimensions and three tracer scenarios:
  `Saved/Automation/20261015-181204-f44513ff`.
- All five physics tests: `Saved/Automation/20261015-181940-c5808233`.
- Private/public equivalence: `Saved/Automation/20261015-182009-variant-2c3c9da5`.
- Driver, iron and slow-shot rendered tracer sequences:
  `Saved/Automation/20261015-182252-variant-98d529d0`; inspected captures under
  `Saved/Screenshots/ShotTrail/20261015-182311-*`. Slow-shot opacity fell from
  0.122 at 1.99 m/s to below 0.001 at 0.12 m/s.

History tracers (2026-10-15): when a ball stops, its tracer turns orange (0.55
opacity) and stays on screen; the last two finished shots are kept and a third
replaces the oldest. The automatic re-tee keeps them; **T** clears all tracers. All
tracers are now 3 cm wide, widening with camera distance (0.004 × distance) so the
far arc no longer thins below a pixel and breaks up. The ball rotates a fixed pool of three ribbons that share
the existing geometry code. `M_ShotTrail` gained a `Color` parameter (white default).
No physics plugin source changed, so no FlightScope rerun was required.
- Editor build passed. Smoke: `Saved/Automation/20261015-194748-20f7ff14`.
- Range tests (12), including the new history checks in `.ShotTrail`:
  `Saved/Automation/20261015-195454-d20981e9`.
- Physics tests (5): `Saved/Automation/20261015-195009-adbcfdb8`.
- Rendered captures not yet inspected.
- Private and public static visual checks passed:
  `Saved/Automation/20261015-183631-variant-8be43815` and
  `Saved/Automation/20261015-183834-variant-36a875cf`.

Twenty stationary frames per view span more than ten seconds after shaders settle.
Comparing first/last frames in the sampled turf region, the private tee's mean
maximum RGB-channel difference fell from 2.7667/255 before the shading correction
to 0.5454/255 afterward (grass off: 0.5396/255). Pixels changing more than three
levels fell from 25.798% to 0.018%; closeup was 0.002%, coverage edge 0.000%.
This indicates substantially reduced shimmer, not bit-identical temporal rendering.
Measurements: `Saved/Automation/20261015-183631-variant-8be43815/GrassStability.txt`.
The public fallback still shows more blade-edge temporal noise (tee region 9.253%
above three levels versus 0.037% with grass off); the private material correction
is not a replacement for the public asset palette. Its functional/rendered tests
pass, but equivalent public-grass shimmer reduction is not claimed.

The first final private tee timing was 25.502 ms with grass versus 8.413 ms off;
other views were about 8.4 ms. An isolated six-view repeat passed at 8.334 ms on /
8.379 ms off, with all other views 8.344–8.576 ms:
`Saved/Automation/20261015-184323-variant-c054289e`. The first slowdown did not
reproduce. These capped editor measurements do not establish GPU headroom or
packaged performance. Historical captures were 1080x492 versus this run's
1629x932, so historical timing is not a controlled resolution comparison.
A longer repeat crashed in Unreal's ImageWrapper screenshot encoder
(`Saved/Automation/20261015-184123-variant-a430bbe1`); the complete stationary
sequences had already passed, and the shorter repeat used `--static-only`.
The initial sandboxed render failed because shader working files could not be
written outside the workspace; successful renders used approved access and waited
for shader compilation. Linux and packaged-game rendering remain untested.

Shot camera framing (2026-10-15): after the hit the camera holds the tee view for
0.5 s (`FollowDelaySeconds`), then follows the ball from 7 m behind and 1.35 m above
it, in flight and at rest (`FollowOffset`). It aims at the ball and tilts up 8°
(`FollowFramingPitch`), which puts the ball in the lower third with the horizon in
view, similar to a GSPro follow-camera screenshot. The values are estimated from one
frame, not measured. Previously the camera followed immediately from 9 m back and
3 m up, centred on the ball. No physics code changed. On Windows the editor build,
smoke (`Saved/Automation/20261015-190406-91c8b8bb`) and all nine `UOpenGolf.Range`
tests (`Saved/Automation/20261015-190417-3de3e315`) pass. No test checks the framing; it has
not been visually reviewed in a rendered session yet. Linux remains untested.

Standalone and packaged startup fix (2026-10-15): `-game` and a packaged Development
build crashed in `AUOpenGolfGameMode::StartPlay` with `Assertion failed: Guid.IsValid()`
(`MapBuildData.cpp:956`). The engine assigns a sky atmosphere's static-lighting GUID only
to non-template components, so the range's default-subobject sky was spawned with an
empty one, and the map's built lighting data looked it up. Editor automation did not hit
this. The range now gives the template sky a fixed GUID. On Windows: editor build, smoke,
`UOpenGolf.Range` (`Saved/Automation/20261015-192354-293952d8`), a 60 s `-game` run and a
45 s run of a fresh `RunUAT BuildCookRun` Win64 Development package all start without the
assert. Packaging is manual; there is no packaging script or test yet.

HUD version label (2026-10-15): the bottom-left corner shows `v` plus `ProjectVersion`
from `Config/DefaultGame.ini`, now `0.0.1` (was `0.1.0`). The Test shots button and the
status box moved up together (`HudScale::BottomPanelGap`) to leave room. Windows: build,
smoke and `UOpenGolf.Range` including `.ShotDisplay` version checks
(`Saved/Automation/20261015-202135-8adb5fa5`). Not yet inspected in a rendered session.

Packaging documented (2026-10-15): README [Package Windows](../README.md#package-windows)
gives the `RunUAT BuildCookRun` command and the editor route. A fresh run of that
command succeeded (`BUILD SUCCESSFUL`, 550 packages cooked, about 1 GB; log
`Saved/Logs/PackageCheck.log`, output `Saved/PackageCheck/`). The manifest shows the
enabled private `ProjectNatureRange` plugin content is cooked into the package, so
packages built with `Plugins/PrivateContent/` present are not distributable. This run
was not launched; Square Home from a package, Shipping and Linux remain unverified.

### Recording reliability, Bluetooth cancellation and tracer CPU work

2026-10-15:
recordings now retain failed saves in memory and retry once per second during Play,
including after recording is off or the device disconnects. Files use a same-directory
temporary write and a non-replacing rename; file indices and session folders survive
failures and recording toggles. The HUD shows unsaved counts and errors. Shutdown
makes one final attempt and reports outstanding records; recovery across Play sessions
or application restarts is not provided.

Shot records associate device and simulation IDs explicitly. Club responses can arrive
before or after ball rest, until three seconds after shot receipt. An interrupted shot
can still accept club data but cannot receive another simulation's result. Stop-recording
and shutdown finalize the data available immediately. Regression coverage includes
actual directory/write/rename failures, no-overwrite behavior, recovery, delayed and
missing club data, session transitions, unrelated results and shutdown failures.

Windows Bluetooth waits now check cancellation within 25 ms, with a 10 s operation
limit and a 30 s device-open/discovery limit. Disconnect invalidates queued work and
callbacks; shutdown cooperatively stops and joins the worker. Cleanup does not wait
for remote notification-disable writes. Initialization errors reach the session error
path. Deadline/cancellation and session tests use deterministic clocks and fake
transports; Windows driver behavior and real-device reconnect/shutdown still need
hardware validation. OS calls during cancellation/resource release can still block.

The game tracer reuses vertex/index/UV buffers, updates existing mesh sections when
topology is unchanged, and skips unchanged geometry, opacity and visibility work.
The previous geometry path is retained only in development automation builds for
comparison. The 120 Hz timestep, 12-substep cap and physics-plugin source are unchanged.
`UOpenGolf.Range.FrameRates` verifies carry, total and simulation duration at 30, 60,
144 and sustained 5 FPS, plus 250 ms hitches. Stalls still discard excess catch-up time.

Windows, Unreal 5.8.3 validation:
- Editor builds pass. Full headless project run: 23/23 passed
  (`Saved/Automation/20261015-192001-1dda1bf0`), including all five physics tests.
- The subsequently added real-filesystem storage regression and both recording tests:
  3/3 passed (`Saved/Automation/20261015-192237-b4f861f8`). Together these runs cover
  all 24 current headless project tests.
- After the final transport cleanup adjustment, all five launch-monitor tests pass
  (`Saved/Automation/20261015-192355-169c33aa`); separate smoke passes
  (`Saved/Automation/20261015-192355-a2fd063a`). All 17 Python tests pass.
- Driver, iron and slow-shot rendered sequences pass
  (`Saved/Automation/20261015-192716-variant-662113bb`). Captures under
  `Saved/Screenshots/ShotTrail/20261015-192740-*` were visually inspected for the
  camera-facing ribbon and rollout fade. These captures used the editor's 1080x492
  viewport, before the timing test was changed to enforce its render-target size.

The final timing workload explicitly sets and checks a 1280x720 render target,
waits ten seconds after shaders finish, disables VSync, caps at 120 FPS, and omits
screenshot encoding. Mean game-thread measurements, in microseconds per actor/widget
tick; frame intervals are milliseconds:

| Shot | Previous tracer | Updated tracer | Reduction | Ball physics | HUD refresh | Previous / updated frame interval |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Driver | 67.185 | 44.731 | 33% | 33.951 | 43.851 | 8.850 / 9.038 |
| Iron | 45.648 | 31.554 | 31% | 28.178 | 44.981 | 9.027 / 9.405 |
| Slow shot | 33.932 | 15.157 | 55% | 44.311 | 43.408 | 8.917 / 8.923 |

Previous-path report: `Saved/Automation/20261015-192920-variant-b07518ea`.
Updated-path repeat: `Saved/Automation/20261015-193253-variant-4fde181d`.
An earlier updated-path run had 51.609 / 43.554 / 9.624 ms frame intervals
(`Saved/Automation/20261015-193043-variant-1f0701b0`); the large slowdown did not
reproduce. Frame intervals are slightly worse in the final updated-path comparison,
and these editor measurements do not establish an FPS improvement, GPU headroom or
packaged performance. They support reduced tracer CPU work. Ball physics and HUD
refresh each average less than 0.05 ms in the final repeat, so neither was rewritten
for performance. Linux and these changes on real hardware remain unverified.
No physics-plugin source or shot-library data changed; no FlightScope rerun was needed.

### History tracer review

2026-10-15: source review of the three-ribbon pool found that retained shots use the
optimized geometry path. Promotion rotates ribbon indices without copying path arrays
or creating components. Each ribbon retains its CPU buffers; camera-only changes use
`UpdateMeshSection_LinearColor`, and a stationary camera/unchanged width skips geometry
work. Topology creation is confined to changed point counts (including a possible
final sample on promotion). History is capped at two ribbons with 4096 points per path.
The allocation/recreation implementation remains opt-in through `-RangeLegacyTrail`
in development automation builds; adding shot history does not select it.

Unreal's procedural-mesh update still copies vertex data for the render thread, so
moving-camera history updates are not allocation-free. Two follow-ups remain: history
with zero opacity is hidden but still receives geometry updates while the camera moves;
and existing history tests verify retention/eviction/clearing but do not assert mesh
creation/update/skip counts. Those counts would guard against future performance regressions.
No runtime source was changed as part of this review.

The latest Windows editor build passes after closing processes that held the module DLL.
Separate smoke and history regression pass:
`Saved/Automation/20261015-195459-ed4ca1f5` and
`Saved/Automation/20261015-195459-1a145e64`. The history regression checks oldest-shot
eviction, automatic re-tee preservation and explicit clearing across three shots.

Sequential rendered timings use the checked 1280x720 target, VSync off, 120 FPS cap,
shader settling and no screenshots. Both runs pass. Means include all tracer work
for the active shot and retained histories, in microseconds per actor tick:

| Active shot | Retained shots during flight/rollout | Optimized tracer CPU | Legacy tracer CPU | Optimized / legacy frame interval, ms |
| --- | ---: | ---: | ---: | ---: |
| Driver | 0 | 43.891 | 67.652 | 9.848 / 13.217 |
| Iron | 1 | 64.706 | 96.827 | 9.653 / 20.146 |
| Slow shot | 2 | 69.620 | 116.897 | 10.191 / 20.619 |

Optimized report: `Saved/Automation/20261015-195558-variant-525fc874`.
Legacy report: `Saved/Automation/20261015-195707-variant-2aa1c571`.
This is a history-enabled comparison, separate from the earlier single-ribbon timing.
The editor measurements support lower tracer CPU work; they do not establish packaged
FPS or GPU headroom. The history review did not verify Linux or packaged rendering.
No physics-plugin source or shot-library data changed.

## Phase 6 — Community and content expansion

**Status:** not started.
