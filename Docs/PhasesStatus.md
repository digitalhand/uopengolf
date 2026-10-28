# Phase status

Where each phase of the [roadmap](Roadmap.md) stands on Windows and Linux. The roadmap
defines each phase's scope and exit criteria. Each phase below gives its current state per
platform and what remains. The [history](#history) keeps every dated entry with its
evidence paths, and [reference measurements](#reference-measurements) keeps the measured
tables. When evidence changes, update the phase summary and add a history line that
includes the engine version, OS and date actually tested.

Test machines:
- **Windows:** Unreal 5.8.4 (CL 58902052), MSVC 14.44.35207, Windows SDK 10.0.26100.0.
- **Linux:** Ubuntu 26.04.1 LTS, kernel 7.0.0-38-generic, prebuilt Unreal 5.8.4
  (bundled Clang 20.1.8 last confirmed with 5.8.3), BlueZ 5.85.

| Phase | Windows | Linux |
| --- | --- | --- |
| 0 — UOpenGolf foundation | Complete | Complete |
| 1 — Linux baseline and integration design | Complete | Complete |
| 2 — Launch-monitor input | Square Home verified; reconnect after link loss and the latest build unverified | Square Home verified on hardware, including reconnect, disconnect and quit; the 2026-10-18 disconnect change awaits a hardware recheck |
| 3 — Native OpenFairway | Core, simulator and ball actor ported; fixtures pass | Same tests pass |
| 4 — Greens and surfaces | Surface groundwork (physical-material surfaces, `OpenGolfCourse` kit) written; not built or run | Surface and cloth tests pass; flat putting green and editable public range saved; green speed setting and green rollout fix checked in Play |
| 5 — End-to-end golf slice | Whitebox range works end to end; `ue.ps1 Package` not yet run | Whitebox range works end to end; `ue.sh package` and its start check pass |
| 6 — Community and content expansion | Not started | Not started |

## Phase 0 — UOpenGolf foundation

**Windows:** the editor target builds, and `UOpenGolf.Smoke.UOpenGolf` passes headless
(2026-10-14). The rendered greeting was never checked visually; the range HUD has since
replaced it.

**Linux:** the editor target builds in Development and DebugGame with no project
warnings, and smoke passes (2026-10-16). VS Code project files and the compilation
database generate. On Linux, Unreal's forced `-TestExit` returns 1 even after a clean run
(`FUnixPlatformMisc::RequestExit`), so `Scripts/ue.sh` lets the report decide for exit
codes 0 and 1. A run that matches no tests still fails.

## Phase 1 — Linux baseline and integration design

**Both platforms:** the [launch-monitor integration design](LaunchMonitorIntegration.md)
covers the pinned revision, the native C++ port decision, units, signs, missing values,
threading, ownership and lifecycle, with Windows (WinRT) and Linux (BlueZ) transports.

The physics baseline has 21 golden fixtures from the ogolf-physics C# sources at
`e6d793a`: seven surfaces, a putt and a spin-back. Tolerances are 0.1 yd for distances,
0.1 ft for apex and 0.01 s for hang time, with a 120 Hz fixed step. See the
[OpenFairway Physics README](../Plugins/OpenFairwayPhysics/README.md). The fixtures pass
on Windows and on Linux, where the full 27-test suite passes
(`Saved/Automation/20261018-095027-AZein3`).

## Phase 2 — Launch-monitor input

The `OpenLaunchMonitor` plugin ports the Square protocol and session to C++. Replay input,
Blueprint events, the HUD status panel and the hardware-free protocol, session,
cancellation and replay tests are the same on both platforms. Launch-monitor errors
show in the HUD status box. On connect, the log records `Square Home firmware <x>.` and
`Battery <n>%.`.

**Windows** (C++/WinRT transport): with a real Square Home, connection, battery and
firmware readout, shot data and club data work (2026-10-14). Club selection and the club
toggle work (2026-10-15), and F9 recording of real shots works (2026-10-15).

**Linux** (BlueZ over runtime-loaded sd-bus, ported from open-lm-connector `655fd96`): on
2026-10-17 the maintainer reported this hardware checklist working with a real Square Home
in the editor:
- connect, battery and firmware;
- shots and club data;
- club change and the handedness toggle;
- F9 recording to `~/Documents/UOpenGolf/Saved/ShotRecordings/`;
- **C**, then **D** within a second, then **C** again;
- reconnect after power-cycling the Square;
- **D**, and quitting while connected;
- Windows connecting again afterwards.

The pairing agent wasn't needed, because the Square accepts unpaired connections. The
hardware log showed every **D** and every quit waiting 2 s for BlueZ's `Disconnect`, so
cleanup now sends `StopNotify` and `Disconnect` without waiting, as Windows does
(2026-10-18). Because BlueZ keeps a released link up for its 2 s disconnect timer, a
reconnect to the same Square within 3 s first waits for `Connected=false`.

**Remaining:**
- Record the firmware version tested.
- On Windows: reconnect after link loss, plus the build and tests of the 2026-10-17/18
  changes.
- On Linux: confirm on hardware that **T** (disconnect, formerly **D**) and quitting no
  longer wait, and that **C** while connected, or **T** then **C** within a second, reconnects without a later drop.
- Known Linux limits are listed in the
  [plugin README](../Plugins/OpenLaunchMonitor/README.md#known-limitations).

Shot recording (F9) saves each real device shot as JSON under
`Documents/UOpenGolf/Saved/ShotRecordings/session_<timestamp>/club_<club>_shot_<n>.json`
(README "Recording shots"). Failed saves are kept in memory and retried once per
second. Files are written to a same-directory temporary and finalized without
replacing an existing file. Windows uses a non-replacing move; Linux uses `link` and
`unlink`, because `rename` replaces the target. Club data may arrive before or after
ball rest, up to three seconds after the shot. Recovery across Play sessions or
restarts is not provided. The regression tests cover real directory/write/rename
failures and no-overwrite behavior on both platforms.

## Phase 3 — Native OpenFairway

**Both platforms:** the `OpenFairwayPhysics` plugin ports the ogolf-physics core
equations, surfaces, ball profile, launch regimes and headless shot loop to C++. Its
Unreal ball actor uses Chaos sweeps and traces for contact; Chaos doesn't integrate the
ball. The following pass on Linux (2026-10-18):
- `UOpenGolf.Physics.Core`, `.Fixtures` (about 0.0001 yd from the C# reference), `.FlightCorrections`,
  `.FlightScopeAnalysis`, `.ShotLibrary` and `.Wind`;
- `UOpenGolf.Range.Shot`, which lands and stops a drive on the running range within
  tolerance of the headless result.

On Windows the same tests passed through 2026-10-15. `.Wind` and the 2026-10-16
drag/lift changes have not been run on Windows yet.

Flight-model corrections include speed-dependent spin decay, interpolated calibration
scales and landing responses, a drag-cap blend and a low-Re lift bridge. Wind, humidity
and pressure are now inputs. The game still defaults to calm, dry air at 75 °F. Accuracy
against FlightScope is tracked in the [FlightScope analysis](FlightScopeAnalysis.md).
The current profile carries long for woods and irons and rolls short.

**Remaining:** JSON ball profiles, slope and terrain contact coverage (the range is flat),
a full ball and camera visual review, and independent physical calibration.

## Phase 4 — Greens and surfaces

**Status:** groundwork only; no ogolf code has been imported. Surfaces can be declared by
`UOpenFairwayPhysicalMaterial` on a material, material instance or Landscape layer, with tags
kept as an explicit override. The `OpenGolfCourse` plugin supplies a physical material for
every surface and `MI_Fairway`/`MI_Rough`, and the range now has a 34 m fairway with
rough that plays as Rough. The public range includes a flat putting green with no mowing
stripes. Contours, sand and more varied lies are not started.

- Linux (2026-10-18, Unreal 5.8.3): the Development editor build and all 33 headless tests
  pass (`Saved/Automation/20261018-200310-joOHPY`). This includes physical-material
  resolution, visual roll, course assets, flag cloth, editor layout, range presentation
  and mixed-surface rollout. An additional playable-bounds test passes
  (`Saved/Automation/20261018-200731-6zwZa5`): shots at both aim limits and a long drive
  land and stop naturally on rough beyond the old boundaries.
- Linux (2026-10-18, later): green speed is a ball setting (`GreenStimpFeet`) with the
  `uopengolf.GreenStimp` console variable. Greens now roll a full-shot run-out farther than
  fairway: the user's replay across the putting green went 161.5 yd, against 159.2 yd on
  fairway. Six physics tests pass; the range rollout tests have not run on this change.
- Windows: not built.

The map saves separate actors for editing and reuses its range controller in Play.
The old mesh flag is removed; one cloth flag responds to the ball's wind and gravity,
without material displacement. The green surround has an opening beneath the putting
disc to avoid competing collision surfaces. Interrupted shots report a safety stop
and omit completed total and roll values. These checks do not establish real-world
cloth accuracy or a contoured course.

## Phase 5 — End-to-end golf slice

**Both platforms:** measured and replayed shots reach native physics on the whitebox
range, which has:
- ball presentation, a white tracer and orange history tracers (the last two shots,
  cleared with **R**);
- a follow camera, the Slate shot HUD and the version label;
- automatic tee reset after 3 s;
- A/D aim (±45°, the camera turns with it) for hitting into the rough;
- nine replay presets (**P**, repeat with **H**) and optional private range visuals.

The `UOpenGolf.Range` tests cover all of these on both platforms.

**Packaging:**
- `bash Scripts/ue.sh package` and `Scripts/ue.ps1 Package` build, cook, stage and archive a
  Development package into `Packaged/<Linux|Windows>/`.
- They then start the packaged game headless for 60 s. `Scripts/check_package_start.py`
  fails the run unless the log shows the startup map loaded and no crash marker.
- **Linux:** the package (1.1 GB, public content only) passes its start check
  (2026-10-18). The latest editable-range package builds, cooks and passes its 60-second
  headless startup check (`Saved/Automation/20261018-203950-package-ihhCZ2`;
  `Saved/Logs/Package.log`).
- **Windows:** a manual `RunUAT` package started without crashing (2026-10-15);
  `ue.ps1 Package` itself has not been run.
- The editor `-game` mode also passes the same start check on Linux.

**Remaining:**
- Run `ue.ps1 Package` on Windows.
- A Square Home shot from a packaged game on each platform.
- Shipping builds, and a real course.
- Visual review of the follow camera, the latest palette and the tee reset in a rendered
  session.
- Equivalent shimmer reduction for the public grass.
- Packaged rendering and FPS; latest Windows captures and timing. Linux editor captures
  and provisional 1080p timing are recorded below. Repeat timing with only one editor.

## Phase 6 — Community and content expansion

**Status:** not started.

## History

Newest first. Evidence paths are relative to the repository root.

- **2026-10-28, Windows, Unreal 5.8.4 upgrade:**
  - The Epic launcher replaced 5.8.3 with 5.8.4 (CL 58902052). `EngineAssociation` stays `5.8`.
  - `ue.ps1 Build` and `Smoke` pass. Report: `Saved/Automation/20261028-105936-133883c9`.
  - The package-runner check now requires 5.8.4 on both platforms.
  - Not yet rerun on 5.8.4: the Linux build and tests, the physics, range and launch-monitor
    tests, the FlightScope comparison and packaging.

- **2026-10-27, Linux and Windows package actions (Unreal 5.8.3):**
  - Split packaging into manual **Package Linux** and **Package Windows** actions, with
    self-hosted x64 build jobs, one package per platform at a time, and shared draft-release
    serialization. Small preparation and release-upload jobs still use hosted Ubuntu.
  - Added checks for engine and build tools, unresolved LFS assets, restricted content,
    local config and running editors. Local editor-tool plugins are optional and editor-only.
  - Linux x64: 71 Python tests pass, including 19 runner checks; actionlint 1.7.12, Bash
    syntax and ShellCheck at warning severity pass. Temporary-file checks pass for Linux
    archives, symbols, checksums and cleanup. Release-helper checks pass with a mocked
    GitHub CLI; no release was created. Actual engine checks use 5.8.3, CL 58210709,
    with Clang 20.1.8; the development checkout correctly fails the private-content check.
  - Report: `Saved/Automation/20261027-ci-packaging-ksi4isvq/validation.txt`.
    Full Linux packaging is pending because a host Unreal editor is running. Native Windows
    checks and packaging, Actions uploads/downloads and a real shared draft remain pending.
    No rendered performance, shimmer or hardware results are established by these checks.

- **2026-10-18, Linux, green speed and rollout (Unreal 5.8.3):**
  - The ball exposes Green Stimp Feet (8–15, default 10). During Play,
    `uopengolf.GreenStimp` sets it, and the shot log reports the stimp used.
  - The current profile's green grass drag is 0.0009, down from the ported 0.011. The
    green's rolling friction is still solved from stimp. `HistoricalReference()` keeps
    0.011, so the golden fixtures are unchanged. Stimp alone did not help: the old drag,
    the same at every stimp, slowed a full-shot run-out on the green more than on fairway.
  - The user replayed the low wood (114.5 mph, VLA 6.95, 121.5 yd carry) at stimp 10 in the
    DebugGame editor. Before: 35.5 yd roll / 157.1 yd across the putting green, 37.7 /
    159.2 yd on fairway. After: 39.9 / 161.5 yd across the green; fairway unchanged. The
    user confirmed the result. Numbers and reasoning are in
    [FlightScope analysis](FlightScopeAnalysis.md#green-and-fairway-by-rolling-speed).
  - The Development build passes. In that editor, through unreal-mcp,
    `UOpenGolf.Physics.Core`, `.Fixtures`, `.GreenRollout`, `.GreenStimp`,
    `.SurfaceResolution` and `.Wind` pass, 6 of 6 (no saved report folder).
    `UOpenGolf.Range.RolloutSurfaces` and `.Presentation`, which this change also edits,
    have not run. Windows has not been checked. Earlier full-suite results do not cover
    this change.

- **2026-10-18, Linux, editor wind controls (Unreal 5.8.3):**
  - Range settings exposes Wind Speed (mph), Wind From (degrees) and Preview Wind in Editor.
    These properties save with the map. The preview uses the range's wind and default air
    density; Play initializes the ball from those values and keeps the flag in the active
    shot's wind. Console changes during Play remain overrides of its range controls.
  - The Development editor build passes. `UOpenGolf.Range.WindSettings` was added for editor
    preview, authored wind surviving into Play, active-shot wind retention, runtime edits
    and console overrides. Execution is pending closure of the interactive DebugGame
    editor. Earlier test and package results below do not validate this addition.

- **2026-10-18, Linux, editable range and green/flag repair (Unreal 5.8.3):**
  - The public map saves its ground, targets and lighting as separate actors. Play reuses
    the saved range, and rebuilding preserves existing actor edits.
  - Green mowing contrast is zero. The pole mesh has one material slot; one simulated
    cloth surface uses a material without vertex displacement. Calm air, 5/10/20 mph,
    wind direction, cloth stretch and 30/144 FPS checks pass.
  - Rough extends beyond the old rectangle. A green surround with an open center removes
    the hidden fairway collision beneath the putting disc. Safety stops are explicit in
    the HUD and recorded results.
  - All 33 headless tests pass (`Saved/Automation/20261018-200310-joOHPY`), followed by the
    new wide/long-shot integration test (`Saved/Automation/20261018-200731-6zwZa5`).
    The Python suite has 35 passing tests. Windows has not been checked for this change.
  - Rendered range and shot-trail checks pass (`Saved/Automation/FinalVisualVerified`).
    Captures include one hanging flag in calm air and one flag flying in a 10 mph crosswind
    (`Saved/Screenshots/RangeAssets/20261018-203022-Flag.png` and
    `20261018-203042-FlagWind10mph.png`). The green has no stripes; fairway stripes remain.
    Launch-direction and replay-selector captures also pass in
    `Saved/Automation/FinalVisual` (that run's shot-trail test was marked failed by an
    unrelated MCP discovery error; the isolated reruns pass).
  - Linux Development packaging and its 60-second headless startup check pass
    (`Saved/Logs/Package.log`, `Saved/Automation/20261018-203950-package-ihhCZ2`).
  - The full asset builder was initially run and re-saved assets outside the requested
    green/flag scope. No untouched binary backup exists for those untracked assets.
    Further repairs use `FixGreenAndFlag.py`; it does not edit rough or fairway materials.
  - Tooling limits observed during validation: a graceful scripted editor exit can crash
    in `TextBiDi::Internal::FICUTextBiDi` / `FTextLayout` cleanup. It also reproduces with
    `/Engine/Maps/Entry`, an empty Python quit script and MCP disabled; see
    `Saved/Logs/ShutdownProbeEmpty-gdb.log`. The final map saved successfully before
    exit, and subsequent editor runs load it. This engine/editor cleanup issue remains
    unresolved. The Linux session also exhausted its 65,536 inotify watch limit while
    watching engine shader directories; no system limits were changed.

- **2026-10-18, Linux, course kit (build only):**
  - Added the `OpenGolfCourse` plugin. It covers surface materials whose physical materials
    declare OpenFairway surfaces, plus a flagstick whose flag waves by world-position offset
    (16 x 8 grid), a ball with an alignment line and dot, and turf grass. Editable `.blend`
    sources are in `Plugins/OpenGolfCourse/SourceArt/`, and the asset scripts moved from
    `Scripts/` into the plugin.
  - The range fairway narrowed from 40 m to 34 m. The side strips use `MI_Rough` and play
    as Rough. Mowing bands now run down the target line: nine 3.78 m bands at half contrast.
  - The ball turns as it rolls along the ground, capped at 1 rad per frame, and shows no
    flight spin.
  - Verified: Unreal 5.8.3 `ue.sh build` (Development editor) succeeds; Blender 5.0.1
    exports all three meshes. Not yet verified: the automation tests, rendered captures,
    moire against the `GrassPalette.v3` horizon-band baseline below, and Windows.

- **2026-10-18, Linux, range flicker:**
  - Cause: the procedural mowing stripes aliased near the horizon. With grass off, about 97% of
    pixels in a 20-pixel horizon band changed every frame. `grass_stability.py` missed it because
    it measured only the lower 40%; it now also reports each view's worst band.
  - Fix: `GrassPalette.v3` filters the stripes by pixel footprint (`M_Fairway`, `M_GrassBlades`).
    Worst band, tee grass off: 20.1% → 0.42%; tee grass on: 21.6% → 0.89%; grass edge 14.5% →
    3.0% (`Saved/Screenshots/RangeAssets/20261016-025624` vs `20261018-111513`; the new run
    captured at 1693x771 instead of 2013x1056). The grass closeup still changes 8.4% in its worst band.
  - A root-to-tip blade gradient was tried and reverted: it doubled the tee view's worst band
    (0.89% → 1.9–2.0%, `20261018-112026`, `20261018-112358`).
  - The tee now uses the turf material in #406B1F. It compiles, but has not been built or
    run yet.
- **2026-10-17/18, Linux** (report folders are stamped in local time, so some read 20261017):
  - Platform parity: `ue.sh package` / `ue.ps1 Package` with a start check, the Linux
    disconnect that no longer waits, clearer Linux errors (bluetoothd not running, Square
    held by another host), and firmware and battery log lines.
  - The non-adaptive unity build passes. The full suite passes 27/27
    (`Saved/Automation/20261018-095027-AZein3`).
  - The Linux package passes its start check
    (`Saved/Automation/20261018-101432-package-tsNC5M`; earlier runs `Saved/Automation/20261018-095249-package-5k0VsV` and
    `Saved/Automation/20261017-212228-package-r1fzqn`, 120 s including the check).
  - Editor `-game` passes the start check (`Saved/Automation/20261017-212339-game-MDtKIt`).
- **2026-10-17, Linux:**
  - BlueZ transport added. HUD launch-monitor errors added on both platforms. Recordings
    finalize with `link`/`unlink`.
  - All 27 tests pass (`Saved/Automation/20261017-141344-bFJszX`), including
    `ShotRecordingStorage`, which previously failed on Linux, and `BlueZBus` against the
    local daemon.
  - Square Home hardware checklist passed in the editor (see Phase 2).
- **2026-10-16, Linux:**
  - Shot-library baseline recorded (see [FlightScope analysis](FlightScopeAnalysis.md)).
  - Drag-cap blend, low-Re lift bridge, and wind/humidity/pressure inputs adopted.
  - Editor builds; smoke passes (`Saved/Automation/20261016-010434-OHEYpb`).
  - The maintainer pressed Play and hit manual shots. The range rendered with the public
    grass, which shimmers at the tee as measured on Windows.
- **2026-10-15, Windows, history tracers and review:**
  - History tracers added: orange, two retained, cleared with **T**; 3 cm tracers that
    widen with distance.
  - Build and smoke `Saved/Automation/20261015-194748-20f7ff14`; range (12)
    `Saved/Automation/20261015-195454-d20981e9`; physics (5)
    `Saved/Automation/20261015-195009-adbcfdb8`.
  - A source review of the three-ribbon pool found retained shots use the optimized path.
    Two follow-ups remain: zero-opacity history still updates geometry while the camera
    moves, and the tests don't assert mesh update counts. Smoke and history regression:
    `Saved/Automation/20261015-195459-ed4ca1f5`, `Saved/Automation/20261015-195459-1a145e64`.
  - History-enabled timing: optimized `Saved/Automation/20261015-195558-variant-525fc874`,
    legacy `Saved/Automation/20261015-195707-variant-2aa1c571`.
- **2026-10-15, Windows, HUD version label and packaging:**
  - HUD version label `v0.0.1`: `Saved/Automation/20261015-202135-8adb5fa5`.
  - Packaging documented. A manual `RunUAT BuildCookRun` Win64 Development package
    succeeded: 550 packages cooked, about 1 GB, log `Saved/Logs/PackageCheck.log`, output
    `Saved/PackageCheck/`. Private `ProjectNatureRange` content was cooked into it, so
    such packages are not distributable.
- **2026-10-15, Windows, recording reliability, Bluetooth cancellation and tracer CPU work:**
  - Recordings: retries, a non-replacing finalize, unsaved counts in the HUD, and
    late club data.
  - Bluetooth waits check cancellation every 25 ms, with 10 s operation and 30 s
    open/discovery limits. Shutdown joins the worker.
  - Tracer buffers are reused. `UOpenGolf.Range.FrameRates` covers 5–144 FPS and hitches.
  - Full run 23/23: `Saved/Automation/20261015-192001-1dda1bf0`. Storage and recording
    3/3: `Saved/Automation/20261015-192237-b4f861f8`. Launch monitor:
    `Saved/Automation/20261015-192355-169c33aa`. Smoke:
    `Saved/Automation/20261015-192355-a2fd063a`.
  - Rendered tracer sequences `Saved/Automation/20261015-192716-variant-662113bb`, with
    captures in `Saved/Screenshots/ShotTrail/20261015-192740-*`.
  - Timing (see [reference measurements](#reference-measurements)): previous
    `Saved/Automation/20261015-192920-variant-b07518ea`, updated
    `Saved/Automation/20261015-193253-variant-4fde181d`, earlier updated run
    `Saved/Automation/20261015-193043-variant-1f0701b0`.
- **2026-10-15, Windows, standalone and packaged startup fix:**
  - `-game` and packaged builds asserted `Guid.IsValid()` (`MapBuildData.cpp:956`) on the
    range's template sky. The sky now has a fixed GUID.
  - Range tests `Saved/Automation/20261015-192354-293952d8`. A 60 s `-game` run and a 45 s
    package run started cleanly.
- **2026-10-15, Windows, shot camera framing:**
  - The camera holds the tee view for 0.5 s, then follows from 7 m behind and 1.35 m up,
    tilted up 8°.
  - Smoke `Saved/Automation/20261015-190406-91c8b8bb`; range
    `Saved/Automation/20261015-190417-3de3e315`.
- **2026-10-15, Windows, range dimensions, calm turf and white tracer:**
  - 42.67 mm ball, a 213.36 cm flag, and a 2 cm white tracer at 0.35 opacity that fades
    below 5 m/s.
  - Smoke `Saved/Automation/20261015-181940-93764773`; range
    `Saved/Automation/20261015-181204-f44513ff`; physics
    `Saved/Automation/20261015-181940-c5808233`.
  - Private/public equivalence `Saved/Automation/20261015-182009-variant-2c3c9da5`
    (iron 85.9318 / 89.0567 yd; low wood 121.5477 / 159.2398 yd).
  - Tracer sequences `Saved/Automation/20261015-182252-variant-98d529d0`, with captures in
    `Saved/Screenshots/ShotTrail/20261015-182311-*`.
- **2026-10-15, Windows, grass stability and timing:**
  - Static checks: private `Saved/Automation/20261015-183631-variant-8be43815`
    (measurements in `Saved/Automation/20261015-183631-variant-8be43815/GrassStability.txt`),
    public `Saved/Automation/20261015-183834-variant-36a875cf`.
  - Six-view timing repeat `Saved/Automation/20261015-184323-variant-c054289e`.
  - A longer repeat crashed in the ImageWrapper screenshot encoder:
    `Saved/Automation/20261015-184123-variant-a430bbe1`.
- **2026-10-15, Windows, range look pass:**
  - −0.5 EV fixed exposure, no motion blur, a 4 km backdrop, `RoughMaterialOverride`,
    and palette-driven private turf.
  - `Saved/Automation/20261015-165632-7e9796dc`; variant
    `Saved/Automation/20261015-165804-variant-99440e8a`.
- **2026-10-15, Windows, optional turf limited to the fairway:**
  - `UOpenGolf.Range` `Saved/Automation/20261015-155931-f69b247a`.
  - Variant `Saved/Automation/20261015-160556-variant-d730eb3d`; rendered
    `Saved/Automation/20261015-160646-variant-ade59700`.
  - `LocalRangeClass` in `Config/UserGame.ini`: `Saved/Automation/20261015-161611-5ed09967`.
- **2026-10-15, Windows, shot recording:** F9 recording. `UOpenGolf.Range.ShotRecording`
  `Saved/Automation/20261015-134115-0067205c`. Real Square Home shots recorded.
- **2026-10-15, Windows, explicit grass colors:** `GrassDarkColor` `#406B1F` and
  `GrassLightColor` `#4D7E20`. `Saved/Automation/20261015-123610-1f202c67`.
- **2026-10-15, Windows, public range visuals:** CC0 Grass 004, a dimpled ball, a
  flagstick, and toggleable 3D grass (`uopengolf.Grass3D`). 16 tests:
  `Saved/Automation/20261015-005300-f9ffd98a`.
- **2026-10-15, Windows, automatic tee reset:** smoke `Saved/Automation/20261015-002821-ae9a3809`;
  range (7) `Saved/Automation/20261015-002837-403bdc2e`; physics (5)
  `Saved/Automation/20261015-003039-024c9808`.
- **2026-10-15, Windows, HUD restyle:** Slate `SRangeHud`, and Disconnect moved from **X**
  to **D**.
- **2026-10-14, Windows, test-scene assignment:**
  - All range ground uses the Fairway tag. The low wood rolls 37.69 yd for a 159.24 yd total.
  - `Saved/Automation/20261014-234851-cf978ea3`; smoke
    `Saved/Automation/20261014-235107-48ad60d0`.
  - Rendered `Saved/Automation/FairwayScene`; screenshot
    `Saved/Screenshots/ReplaySelector/20261014-235117-5.png`.
- **2026-10-14, Windows, rollout diagnosis:** `UOpenGolf.Range.RolloutSurfaces` reproduces
  the mixed-surface 31.92 yd roll. `Saved/Automation/20261014-234231-61fb0e80`; smoke
  `Saved/Automation/20261014-234420-030290be`.
- **2026-10-14, Windows, FlightScope shot library:**
  - 222 recorded shots (175 with FlightScope results) from openfairway `4f3e11b`.
  - First run `Saved/Automation/20261014-234524-c549296e`. Physics and smoke:
    `Saved/Automation/20261014-234602-50efffc4`, `Saved/Automation/20261014-234614-3123feb3`.
- **2026-10-14, Windows, replay presets:**
  - Nine presets. Disabling the legacy low-launch boost gives 121.55 yd carry (FlightScope
    119.8 yd).
  - 12 tests `Saved/Automation/20261014-233128-032069a4`; selector
    `Saved/Automation/ReplaySelectorFinal`; smoke `Saved/Automation/20261014-233523-38e15ca2`.
- **2026-10-14, Windows, flight-model corrections:**
  - Speed-dependent spin decay and interpolated scales. The screenshot shot carries
    85.94 yd (was 82.69; FlightScope 87.6).
  - 11 tests `Saved/Automation/20261014-230126-b2e8eb64`; smoke
    `Saved/Automation/20261014-230219-11ff00ec`; rendered
    `Saved/Automation/LaunchDirectionsCached`.
- **2026-10-14, Windows:** Square Home connected on hardware. Launch-monitor plugin,
  physics plugin and fixtures, and smoke added.

## Reference measurements

Provisional RTX 3060 measurements, Linux, Unreal 5.8.3 Development editor (2026-10-18),
1920x1080 output, 100% screen percentage, VSync off, no screenshot overhead during shots.
The final process check found an interactive DebugGame editor had started at 20:25:39,
during the earlier visual run, and remained open during these benchmarks. GPU load was
therefore not isolated; repeat with one editor before claiming the 60 FPS target or an
improvement caused by this setting alone.

The 200% TSR-history shot-only run `Saved/Automation/ShotTiming1080` recorded
18.3–18.6 ms p95. The 100% history run `Saved/Automation/ShotTiming1080History100`
passes its functional checks, retaining the other AA settings and all surface materials.
`Config/DefaultEngine.ini` now selects 100% history. Its CSV intervals give:

| Shot | Retained trails | Mean frame (ms) | p95 frame (ms) | Mean GPU (ms) | p95 GPU (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Driver | 0 | 8.737 | 10.203 | 6.547 | 14.510 |
| Iron | 1 | 8.341 | 8.338 | 6.036 | 6.765 |
| Slow | 2 | 8.343 | 8.338 | 6.308 | 7.338 |

The CSV and extracted values are in `Saved/Profiling/CSV/Profile(20261018_203445).csv`
and `Saved/Automation/ShotTiming1080History100/Measurements.json`. These observations
do not establish standalone or packaged FPS, or performance on other hardware.
Lower history resolution can reduce temporal edge detail relative to
the 200% history setting. Output resolution is unchanged.

FlightScope shot library, first run (Windows, 2026-10-14,
`Saved/Automation/20261014-234524-c549296e`), engine minus FlightScope over 175 shots:

| Profile | Carry bias / avg abs (yd) | Carry within 3 / 5 yd | Total bias / avg abs (yd) | Roll bias (yd) | Apex bias / avg abs (ft) | Pass / moderate / severe |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Current | +2.5 / 3.8 | 55% / 71% | −0.4 / 4.9 | −2.9 | −0.1 / 3.2 | 68 / 54 / 53 |
| Historical | +0.1 / 1.8 | 80% / 97% | +3.6 / 5.8 | +3.5 | −1.0 / 3.4 | 80 / 61 / 34 |

Tracer CPU, single ribbon (Windows, 2026-10-15). The figures are mean game-thread µs per
actor or widget tick, with frame intervals in ms. The run used a 1280x720 target, VSync
off, a 120 FPS cap and no screenshots:

| Shot | Previous tracer | Updated tracer | Reduction | Ball physics | HUD refresh | Previous / updated frame interval |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Driver | 67.185 | 44.731 | 33% | 33.951 | 43.851 | 8.850 / 9.038 |
| Iron | 45.648 | 31.554 | 31% | 28.178 | 44.981 | 9.027 / 9.405 |
| Slow shot | 33.932 | 15.157 | 55% | 44.311 | 43.408 | 8.917 / 8.923 |

Tracer CPU with history (Windows, 2026-10-15):

| Active shot | Retained shots | Optimized tracer CPU | Legacy tracer CPU | Optimized / legacy frame interval, ms |
| --- | ---: | ---: | ---: | ---: |
| Driver | 0 | 43.891 | 67.652 | 9.848 / 13.217 |
| Iron | 1 | 64.706 | 96.827 | 9.653 / 20.146 |
| Slow shot | 2 | 69.620 | 116.897 | 10.191 / 20.619 |

These editor measurements support lower tracer CPU work. They don't establish packaged
FPS or GPU headroom.

Grass stability (Windows, 2026-10-15): this compares the first and last of twenty
stationary frames per view. At the private tee, the mean maximum RGB-channel difference
fell from 2.7667/255 to 0.5454/255 (grass off: 0.5396/255). Pixels changing by more than
three levels fell from 25.798% to 0.018%. The public fallback still shows 9.253% at the
tee, versus 0.037% with grass off.

## Trees, trunk colliders and SM6 rendering — 2026-10-19

Graphics work now targets a private range built from locally licensed packs until a
baseline is approved; the public range follows (see [licensing](Licensing.md#current-graphics-work)).

- **Kit.** The OpenGolfCourse kit places trees in vegetation areas with hidden trunk colliders. The ball deflects off `OpenFairway.Obstacle` components and never rests on them. See [Coastal terrain](CoastalTerrain.md#trees-and-trunk-colliders--2026-10-19).
- **Tests.** Unreal 5.8.3, Linux, DebugGame editor via MCP: 10 passed.
  - `UOpenGolf.Course.Terrain.Definition`, `.Scatter`
  - `UOpenGolf.Physics.Core`, `.Fixtures`, `.ObstacleResponse`, `.SurfaceResolution`
  - `UOpenGolf.Range.CoastalTerrain`, `.ObstacleContact`, `.PlayableBounds`, `.Shot`

  These cover the code before the later narrow-area grid and empty-area fixes. Those fixes compile with the Development build and are not yet tested.
- **Rendering.**
  - Linux now targets Vulkan SM6 only, so Nanite runs. Under SM5, Nanite meshes drew their full fallback meshes.
  - Virtual shadow maps are enabled.
  - Provisional editor PIE timings with about 4,660 Nanite trees in a private test layout, RTX 3060, internal resolution about 1368×764, one editor:

    | Configuration | FPS | GPU frame time |
    | --- | ---: | ---: |
    | SM5 | 23 | 41 ms |
    | SM6 | 43 | 22 ms |
    | SM6 + virtual shadow maps | 80 | 11.5 ms |

  - These are not 1080p or packaged results. Earlier SM5 timings no longer describe the current renderer.
- **Not covered.** No shimmer comparison was made for the trees. Windows and packaged runs are unverified.

## Oak wood collision — 2026-10-19

The saved private range now uses hidden collision matching the oak's trunk, solid
branches and cut ends, replacing the short cylindrical proxies. All 296 visible oaks
retain their original transforms and materials. Leaf cards and nuts have collision
disabled. The source pack mesh is unchanged; a private copy supplies the collision.
Both private rebuild scripts reapply this treatment after generating vegetation.

Unreal Engine 5.8.3, Linux DebugGame, one editor: ball-sized sphere sweeps through
upper-trunk and branch faces missed all 592 probes before the change and hit all
592 afterward. Another 592 sweeps through leaf-only canopy and open space remained
clear. Two short airborne ball impacts against actual saved oak wood reflected and
slowed to 5.35 and 5.74 m/s from a 40 mph launch. These are contact checks, not full
tee-to-rest shot or performance measurements. Applying the collision a second time
preserved the result. The saved-map terrain and obstacle-contact automation checks
passed after that reapplication, along with `UOpenGolf.Visual.CoastalTerrain`.
The final sequence contains 200 frames in
`Saved/Automation/CoastalReview/OakWoodCollision-20261020-050026`. Tee and close
frames were inspected: the collision proxies stay invisible. Cameras, output size
(1749 × 977) and AA settings match the preceding desert capture; internal render
resolution remains unverified. `Scripts/grass_stability.py --profile coastal` measured
all six stationary grass-on/off sequences, 20 frames each. Percentages are pixels
changing by more than 3/255, averaged over consecutive frame pairs:

| View | Lower crop before | Lower crop final | Worst band before | Worst band final |
| --- | ---: | ---: | ---: | ---: |
| Tee, grass on | 0.000% | 0.000% | 3.210% | 2.175% |
| Tee, grass off | 0.001% | 0.000% | 3.496% | 2.233% |
| Edge, grass on | 0.012% | 0.012% | 3.166% | 3.169% |
| Edge, grass off | 0.013% | 0.012% | 3.276% | 3.044% |
| Close, grass on | 0.003% | 0.003% | 4.684% | 4.428% |
| Close, grass off | 0.000% | 0.000% | 5.049% | 5.019% |

Here “before” is the completed desert/bunker capture immediately preceding this
collision change. The lower crop is the bottom 40%; the worst band is the most
changed 5% strip. Final worst bands are rows 50–55% at the tee, 30–35% at the edge
and 45–50% close up. Full results are in `Stability.txt` beside the collision report.
No visible art or rendering setting changed, and these runs do not establish a
shimmer improvement from collision. Tree/horizon flicker remains. View 9 and
`grass-motion.mp4` retain slow camera motion; continuous playback review and
controlled FPS remain pending. The editor paused in the debugger after all three
automation checks completed; no second editor was started.

Evidence is in `Saved/Automation/PrivateRange/OakCollision`: `before.json`, `after.json`,
`saved.json`, `LiveShots.json`, `ImplementationChecks.json` and `Editor-final.log`.
The first full private-content backup is
`Saved/TerrainBackups/20261020-045709-oak-wood-collision`. Five existing private
layout tests, Python syntax checks and `git diff --check` pass. No C++ or C# changed
in this collision follow-up.

## Oak understory — 2026-10-20

The private range now has dry-savanna ground cover around each of its 296 oaks:

- leaf-litter patches and dead moss near the trunks
- straw grass tufts at the canopy edge
- sparse small rocks and seedlings

The pieces come from a locally licensed ground-cover pack. They are prepared in an editable
Blender project and imported into ignored private content. A seeded per-oak layout keeps
cover at least 2 m from play surfaces, and the private rebuild scripts reapply it after
every vegetation bake. The cover has no collision, no shadows and no vertex motion. It
follows the 3D grass switch. No C++, public map or tracked configuration changed.

Unreal Engine 5.8.3, Linux DebugGame editor, one editor via MCP, with `uopengolf.TestMap`
set to the private map:

- **Saved map:** 18,185 understory instances in 20 instanced-mesh components on the baked
  ground-cover actor. They reloaded intact after an editor restart: 7,692 grass, 1,702 shade grass, 6,838 litter patches, 1,287 moss,
  417 seedlings and 249 rocks. No point fell on a slope steeper than 30°.
- **Native tests passed:**
  - `UOpenGolf.Visual.CoastalTerrain`: the before run, and both after runs
  - after the final placement: `UOpenGolf.Range.CoastalTerrain`, `UOpenGolf.Range.ObstacleContact`
    and `UOpenGolf.Range.BunkerProbe`
- **Python unit tests passed:** 13 layout tests and 11 export-rule tests.
- **Shimmer.** `Scripts/grass_stability.py --profile coastal` compared the before and final
  sequences: 20 frames per view, 1749 × 977, same cameras and AA. Values are pixels changing by
  more than 3/255, averaged over consecutive frame pairs:

  | View | Lower crop before | Lower crop after | Worst band before | Worst band after |
  | --- | ---: | ---: | ---: | ---: |
  | Tee, grass on | 0.001% | 0.001% | 3.382% | 3.617% |
  | Tee, grass off | 0.001% | 0.000% | 3.516% | 3.093% |
  | Edge, grass on | 0.013% | 0.022% | 3.298% | 3.307% |
  | Edge, grass off | 0.012% | 0.012% | 3.168% | 3.210% |
  | Close, grass on | 0.003% | 0.003% | 4.573% | 4.947% |
  | Close, grass off | 0.000% | 0.000% | 4.861% | 5.001% |

  The worst bands are the horizon rows, 30–50%. The grass-off views hide the understory,
  and between runs they moved by −0.42 to +0.14 points. The grass-on changes, from +0.01
  to +0.37 points, are of similar size. These runs neither show a clear regression nor
  demonstrate an improvement. Tree/horizon flicker remains.
- **Frame time.** Provisional editor PIE frame intervals at the standard probe camera:
  - 3D ground cover hidden: mean 9.72 ms, p95 11.35 ms
  - shown: mean 10.01 ms, p95 11.62 ms

  GPU time was not measured. This is not a controlled 1080p or packaged result.
- **Not covered.**
  - The standard cameras include no oak close-up, and the litter culls from 34 m. Views
    3–9 m from a trunk were reviewed only in editor viewport captures.
  - Continuous slow-motion playback review is pending.
  - Windows, packaged runs and Shipping are unverified.

Evidence: `Saved/Automation/CoastalReview/BeforeUnderstory-20261020-210931`,
`AfterUnderstoryTufts-20261020-211801` and
`Saved/Automation/PrivateRange/GraphicsProbe-20261020-212150`. Private-content
backups: `Saved/TerrainBackups/20261020-211144-understory` and `20261020-211730-understory`.

### Blender sources moved out of the repository — 2026-10-20

Blender projects now live outside the repository under the Blender art root
(`Plugins/OpenGolfCourse/Scripts/art_root.py`, five unit tests). The repository keeps only
Unreal-ready assets, and `.gitignore` excludes `.blend` files.

- **Course kit.** The seven `.blend` sources were moved byte-identical: SHA-256 checked.
  - Re-exporting from the new location opened every project rather than rebuilding it.
  - The FBX files are 16–32 bytes smaller only because Blender embeds the source path.
  - A controlled test exporting one `.blend` from two folders changed the size the same way.
- **Understory assets.** They re-export from the moved project with identical meshes,
  triangle counts, bounds and texture roles.
  - Their texture sources are now 2K colour and normal maps and 1K masks, down from up to
    8K.
  - After the reimport, the saved understory still had 18,185 instances.
  - `UOpenGolf.Visual.CoastalTerrain` passed on the private map
    (`AfterBlenderMove2K-20261020-220001`).
  - Stability matched the previous capture within run-to-run noise:
    - the lower crop moved by 0.002 points at most
    - worst bands were 3.592% tee, 3.126% edge and 4.872% close with grass on, against 3.617%,
      3.307% and 4.947%

## Desert vegetation implementation checkpoint — 2026-10-19

The private range is saved with desert outer ground, green rough beside the maintained
turf, 296 cork oaks and 19,716 grass clumps from four included meshes. The earlier bake
had 4,659 pines and 69,364 grass instances. The oaks use 36 irregular planting patches,
including 12 beside the rough, with seeded random placement and minimum separation.
Each saved oak has a matching trunk collider. The fairway and green materials are
unchanged. Licensed assets and their derivatives remain in ignored private content.

The bunker and its rim and bowl moved from (146, 28) m to (108, 0) m. Its entire
outline is now before the green: the back edge is at 116.27 m and the green begins
at 122.9 m. The green outline and tee XY are unchanged. Terrain, grass and trees
were rebuilt after the move. The full private Content directory was backed up before
the graphics work and again before moving the bunker; the latter backup is
`Saved/TerrainBackups/20261020-044456-bunker-approach`.

Unreal Engine 5.8.3, Linux: the Development build and changed-file compile checks pass
(six files initially, with the three bunker-related files checked again after that edit).
The owner's rebuilt DebugGame editor passed eight final tests through MCP:
`UOpenGolf.Course.Terrain.Definition`, `.RandomScatter`, `.Scatter`,
`UOpenGolf.Physics.ObstacleResponse`, `UOpenGolf.Range.BunkerProbe`,
`UOpenGolf.Range.CoastalTerrain`, `UOpenGolf.Range.ObstacleContact`, and
`UOpenGolf.Visual.CoastalTerrain`. Fifteen Python tests pass: five private
layout/mask/bunker checks and ten shimmer-tool checks. No coverage percentage was measured.
Evidence: `Saved/Automation/DesertBaseline-20261020/ImplementationChecks.json` and
`Editor-final-tests.log`. The MCP result request timed out after the editor paused at
session completion; the retained log records all eight successful results.

Vegetation presets now support seeded random placement with minimum separation and
bounded retries; existing presets retain their grid pattern. Coastal contact, visual,
and obstacle-contact tests can select another map through `uopengolf.TestMap`.
Coastal captures include stationary tee, edge, and close views with grass on/off,
plus a camera-motion sequence; use `Scripts/grass_stability.py --profile coastal`.

The private material generator and saved assets use the same grass treatment: upward
world-space shading normals on both faces, ground-matched colors, high roughness,
instanced-mesh support, no shadows and no vertex motion. The detailed rough blends
green to sand/dirt using a mipmapped distance mask. Grass density falls across three
bands from the maintained turf into the desert. Green `StripeContrast` remains zero.

Rendered evidence uses 20 stationary frames per view at 1749 × 977, FOV 65, TSR,
history screen percentage 100 and automatic screen percentage. Internal render
resolution was not verified. Before and final runs used the same cameras and settings,
one editor at a time; the editor was restarted between runs. Final captures include
the bunker relocation. Frame sequences:
`Saved/Automation/CoastalReview/BeforeDesert-20261020-042731` and
`Saved/Automation/CoastalReview/FinalDesertBunker-20261020-044657`.
The intermediate `AfterDesert` sequence predates the bunker move.

Stationary measurements from `Scripts/grass_stability.py --profile coastal` average
19 consecutive frame pairs. Values below are percentages of pixels changing by more
than 3/255, rounded to three decimals. The lower crop covers the bottom 40% of the
image; the worst band is the most changed horizontal 5% strip.

| View | Lower crop before | Lower crop final | Worst band before | Worst band final |
| --- | ---: | ---: | ---: | ---: |
| Tee, grass on | 0.001% | 0.000% | 4.286% | 3.210% |
| Tee, grass off | 0.000% | 0.001% | 2.783% | 3.496% |
| Edge, grass on | 0.012% | 0.012% | 5.381% | 3.166% |
| Edge, grass off | 0.006% | 0.013% | 5.028% | 3.276% |
| Close, grass on | 0.006% | 0.003% | 2.007% | 4.684% |
| Close, grass off | 0.002% | 0.000% | 1.166% | 5.049% |

The worst bands are at rows 45–50% for tee/close and 30–35% for edge, crossing
trees and the horizon. Tee-on and both edge bands improved, while tee-off and both
close bands worsened. Similar residual change with grass hidden means these readings
do not isolate grass as its source. The scene is not established as shimmer-free.
Full mean-difference and first/last figures are retained in `BeforeStability.txt`
and `FinalStability.txt` beside the test report.

Slow camera motion was captured separately as view 9. Frames 00, 09 and 19 were
inspected for obvious trails, but continuous playback review remains pending.
A local review clip is
`Saved/Automation/PrivateRange/DesertLayout/grass-motion.mp4`.
Controlled FPS, Windows and packaged checks remain pending; these captures establish
neither a frame-rate improvement nor the removal of all shimmer.

## Coastal terrain foundation checkpoint — 2026-10-19

The reusable OpenGolfCourse terrain framework builds with Unreal Engine 5.8.3 on
Linux. Its definition-validation test passed (one test), with report at
`Saved/Automation/20261019-160853-oOEBzC/index.json`; build log is
`Saved/Logs/Build-editor.log`. Two original Blender grass sources were exported.
Asset import and range migration remain unexecuted. No new terrain collision,
rendered quality, Windows build or FPS result is established by this checkpoint.
Work is paused for user review. See [Coastal terrain delivery](CoastalTerrain.md)
for the completed work, limitations and next delivery phases.

## Coastal assets and shimmer check — 2026-10-19

The public map now saves the reusable terrain and baked vegetation. Unreal 5.8.3,
Linux: definition, saved terrain contact and rendered coastal tests passed in
`Saved/Automation/CoastalAfter/index.json`. Build log: `Saved/Logs/Build-editor.log`.
The before-run contact test exposed an overly tight 3 cm rough interpolation check;
the recorded final test allows 10 cm for the coarser rough and retains 3 cm for
playing surfaces. No physics constants changed.

Twenty stationary frames per view were compared at 1745 × 971, with matching cameras
and rendering settings, one editor, and no grass wind. The table shows the average
percentage of pixels changing by more than 3/255 in the worst horizontal band:

| View | Before | After |
| --- | ---: | ---: |
| Tee, grass on | 7.297% | 0.709% |
| Tee, grass off | 0.262% | 0.282% |
| Close grass | 10.523% | 1.215% |
| Grass edge | 6.644% | 0.940% |

The fix uses upward world-space blade normals, a lower-contrast turf-related palette,
high roughness, low specular, and a quieter tee material. Procedural stripe contrast
fades with its pixel footprint. Existing green stripe contrast remains zero.
Measurements: `Saved/Automation/CoastalReview/Stability.json`; frames: sibling
`Before` and `After` folders. These show substantial stationary shimmer reduction,
not elimination of all temporal change. Slow camera motion, Windows, full gameplay
regressions and packaged FPS remain unverified. The first Python integration editor
saved successfully but crashed during shutdown; the final automation loaded and
validated the saved map successfully. See [Coastal terrain delivery](CoastalTerrain.md).

## Jacaranda preparation and oak baseline — 2026-10-20

Unreal Engine 5.8.3, Linux, one running DebugGame editor. The existing map still
has 296 oaks and 296 oak wood colliders. The approved migration now works in place
and requires the new `TreeCollisionMesh` preset field; it stops before map edits
if that field is not loaded. The owner must rebuild DebugGame and restart before
application. The Development build and three changed-file compile checks passed
(`Saved/Logs/Build-editor.log` and `Saved/Logs/Jacaranda-single-*.log`).

A hidden jacaranda proxy was saved with Nanite off and collision enabled only on
trunk and branch sections. Visible jacaranda Nanite stays on. A recursive asset
dependency walk visited 18 packages without finding an unexpected content root
(`Saved/Automation/Jacaranda/preparation.json`). This does not establish collision
in the range. The matching canopy ratio is 0.5368158485. CC0 notice and legal text
are present in the public art plugin. No ground, grass or lighting assets changed.

`UOpenGolf.Visual.CoastalTerrain` passed on the oak map and saved 200 frames in
`Saved/Automation/CoastalReview/BeforeJacaranda-20261021-004252`. The frames are
1320 × 959, FOV 65, with the existing rendering settings. Background CPU throttling
was temporarily disabled for capture. Measurements use 19 consecutive pairs,
threshold 3/255, bottom 40% and the worst 5% horizontal band:

| View | Lower crop | Worst band |
| --- | ---: | ---: |
| Tee, grass on | 0.001% | 5.565% |
| Tee, grass off | 0.001% | 5.354% |
| Edge, grass on | 0.025% | 5.255% |
| Edge, grass off | 0.010% | 5.029% |
| Close, grass on | 0.005% | 6.380% |
| Close, grass off | 0.000% | 6.040% |

Full output: `Saved/Automation/Jacaranda/BeforeStability.txt`. These are before
measurements, not evidence for the replacement. A tee frame was inspected; slow
motion playback and tree close-ups remain pending. Final jacaranda sequences,
actual wood/leaf sweeps, airborne and low shots, reopen/rebuild checks, native
runtime tests for the new field, controlled frame times and owner parity review
remain pending. No FPS or shimmer improvement is claimed.

Python checks passed: six fetch/migration-rule tests, six export-rule tests and
13 private layout tests. Changed scripts compile. No coverage percentage was
measured. No C# changed; this checkout has no `OpenFairway.sln`.

## Jacaranda applied in place — 2026-10-20

Unreal Engine 5.8.3, Linux, one DebugGame editor (rebuilt with `TreeCollisionMesh`). The
private range `/UOpenGolfPrivateRange/Maps/UOpenGolf_Private` now has 296 jacarandas and
296 hidden wood colliders. All 36 tree presets point at `SM_Jacaranda` with
`TreeCollisionMesh = SM_JacarandaCollision`, and their original oak scale ranges are kept in
metadata. Canopy ratio 0.5368158485. Backup with the oak `trees-before.json`:
`Saved/TerrainBackups/20261021-011543-628849-jacaranda`. Report:
`Saved/Automation/Jacaranda/migration.json`.

| Check | Result |
| --- | --- |
| Trees / colliders | 296 / 296, one per tree, matching transforms |
| Wood sweeps (trunk, branches) | 592 of 592 hit the tree's own collider |
| Leaf and open-gap sweeps | 592 of 592 clear |
| Proxy sections | Trunk and branches collide; leaves do not; proxy Nanite off |

Fixes found while applying:

- **Editor crash.** The first attempt froze the editor about 7 s while rebuilding the
  804k-triangle proxy, and the Wayland compositor closed the connection (`LogSDL3: Wayland
  display connection closed by server (fatal)`). Nothing was saved. `proxy_mesh` now rebuilds
  the proxy only when its triangle count or bounds differ from the tree's.
- **Python API gaps.** In this engine, `Vector.equals` takes no tolerance and
  `GameplayStatics.break_hit_result` doesn't exist. `check` compares components directly and
  reads the hit component from `HitResult.to_tuple()`.
- **Probe faces.** For a Nanite mesh, Python only sees the 5k-triangle fallback, so the probes
  now come from the full-geometry proxy. Wood probes sweep both ways through the face, because
  the imported winding can point a trunk normal inward.
- **Recovery.** The swap and presets came from `SpikeJacaranda.py`. Its final check, save and
  report steps were finished by a one-off script after these fixes.
- **Review capture.** `CaptureTreeReview.py` could not spawn a camera in Play, and the range
  resets its camera every tick, so its tee-view results are invalid (kept in
  `invalid-tee-view/`). It now refuses a shot that is not at its pose.

The close-ups are editor viewport captures (MCP, FOV 90) at the poses in `review-poses.json`:
`Saved/Automation/Jacaranda/before/`, `after/` and `oak-vs-jacaranda-closeups.jpg`.

Open issues:

- Distant jacarandas render as bare branches. The mesh has Nanite `shapePreservation: None`;
  masked leaf cards need `PreserveArea`. Applying it rebuilds Nanite for 804k triangles, so it
  waits for the owner because of the Wayland freeze risk.
- The foliage reads paler than the oak, so leaf tint needs owner review.
- Branch collision is too broad: small branches deflect the ball sharply. The owner wants
  trunk-only collision, recorded as [technical debt](Roadmap.md#technical-debt).

Pending:

- the rebake check (`BuildPrivateRange.py --trees-only` plus a layout comparison)
- actual shots in Play
- native range and terrain tests
- `UOpenGolf.Visual.CoastalTerrain` sequences, shimmer and frame time
- owner parity review

No shimmer, frame-time or gameplay result is claimed for the jacaranda yet.

## Rough boundary and fairway validation checkpoint — 2026-10-21

The owner requested continuous natural rough-edge cover and a Grass008 fairway trial,
reusable on future courses. The putting green and its discoloration are outside scope.

Implemented:

- `ScatterBoundary` samples rough-side region edges with seeded spacing, slope and
  clearance checks and exclusions for existing coverage. The editor builds PCG points
  and replaces only the previous boundary fill after baking succeeds.
- Saved `DA_BoundaryGrass` uses the existing open Grass Medium mesh family. Existing
  meshes, grass materials, vegetation and map files were not changed.
- Imported the pinned 2K Grass008 color, normal and roughness maps, a dedicated fairway
  material parent/instance and `DA_FairwayGrass008`. No shared green material was edited.
- Added scoped application, asset-generation and download scripts, plus course-kit and
  savanna README instructions. The application backs up the map and terrain definition.

Validation: Unreal Engine 5.8.3, Linux, Development editor build succeeded. All four
`UOpenGolf.Course.Terrain` tests passed, including the new boundary placement test.
Report: `Saved/Automation/BoundaryFairwayNative/index.json`. Python scripts compile;
`git diff --check` passes. No C# source was changed.

Before-only rendered capture passed with one editor:
`Saved/Automation/CoastalReview/BeforeBoundaryFairway-20261021-201637` (200 frames).
Measured capture size was 1320×959, despite the requested window size; this is not a
controlled 1080p performance result. Stability output:
`Saved/Automation/BoundaryFairwayBefore/stability.txt`.

| View | Lower-crop changed pixels | Worst horizontal band |
| --- | ---: | ---: |
| Tee, grass on | 0.001% | 10.339% |
| Tee, grass off | 0.001% | 10.487% |
| Edge, grass on | 0.029% | 10.247% |
| Edge, grass off | 0.013% | 9.938% |
| Close, grass on | 0.000% | 7.307% |
| Close, grass off | 0.000% | 7.065% |

Pending owner validation: neither saved range map contained the new GrassMedium
placements during inspection. Both saved maps were left untouched. Save the intended
current grass layout before applying the scoped script. PCG bake/regeneration integration,
final coverage, fairway appearance, after-change shimmer sequences, continuous slow-motion
review, saved-terrain/rollout tests and comparable frame times remain pending. The native
placement test and asset import do not establish those results. No shimmer improvement or
visual parity is claimed. The owner requested this checkpoint before further work.

## Course grass and surfaces applied — 2026-10-21

The owner asked for full Grass Medium cover on the rough beside the fairway with no gaps,
Ground093C on the bunkers, and a fix for a lighter patch on the green. This supersedes the
pending items of the boundary/fairway checkpoint above. Applied to the private map with the
DebugGame editor (UE 5.8.3, Linux, one editor):

- **Rough:** a seamless 3D grass layer from `ApplyCourseGrass.py`, with backups under
  `Saved/TerrainBackups/*-course-grass`. The final run (`20261021-224047-284664`) placed
  15,006 pieces:
  - 1 m, 0.5 m and 0.25 m tiles, edge to edge in quarter turns, from the outlines out to 12 m
  - 891 edge strips along the outlines

  Scattered 1 m tiles from 15 to 40 m were tried and removed at the owner's request; on bare
  soil they read as spots. The private rough mask is now solid to 12 m and fades to desert by
  16 m (it was 6 m and 24 m). The rough underlay uses Poly Haven `leafy_grass` at about half
  its earlier brightness, so it matches the rendered blades.
- **Surfaces:** Grass008 on the fairway, Grass008 lighter and finer with no stripes on the
  green, and Ground093C on the bunkers. All share `/OpenGolfSavanna/Materials/M_SavannaSurface`.
  The physical materials are unchanged (`PM_Fairway`, `PM_Green`, `PM_Sand`).
  - The first build showed no texture: like the kit's palette graph, it kept only the
    texture's luminance, which moved the colour by about 1%.
  - The colour node now scales the texture so its mean lands on `TargetColor`.
- **Green discoloration:** the cause was the surface material's 19 m `MacroVariation` noise,
  not geometry. The green mesh has 0.5 m vertex spacing.
  - The `frac(sin(x) * 43758)` hash let neighbouring cells disagree on shared corners. That
    drew straight seams along the cell grid; one corner sits near the hole, splitting the
    green into quadrants.
  - The noise now hashes integer cell coordinates (`BuildTerrainAssets.MACRO_VARIATION`).
  - `FixMacroVariation.py` repaired the saved `M_CoastalSurface` without the full kit build.
  - A same-pose capture shows the seams gone from the green, fringe and fairway.
- **Fixes found during application:**
  - `ApplyCourseGrass.py` guards its tick callback against re-entry. An unguarded import
    driver looped when imports pumped Slate, and the editor had to be killed; nothing was
    saved by that loop.
  - Per-mesh counts in its log are now summed across presets.
  - The private plugin declares its `OpenGolfSavanna` dependency. This takes effect at the
    next editor start; until then the asset validator flags `MI_PrivateDesertRough`.

Validation:

- All five `UOpenGolf.Course.Terrain.*` tests passed in the running editor through MCP,
  including the new TiledGrass and BoundaryGrass tests.
- The private layout tests and the savanna Blender rule tests pass.
- Same-pose stills are in `Saved/Screenshots/CourseGrass/`: `before_*`, `after_*`,
  `after2_*` (surfaces) and `final3_*` (12 m band). They show full cover beside the fairway at
  0.9–4 m and textured fairway, green and bunker. The captures are single-frame MCP renders
  at 1320×959; thin objects such as the flagpole can drop out.

Not yet verified:

- shimmer sequences (`grass_stability.py`) and continuous slow-motion review
- frame time (`RangeGraphicsProbe.py`)
- Nanite `PreserveArea` on the tiles
- saved-terrain contact, bunker and rollout tests, and a Play check

Known limitation, recorded as [technical debt](Roadmap.md#technical-debt): seen from a
distance, the edge row beside the fairway reads as a darker band. Owner review is pending.

## Open desert ground and Nature Essentials move — 2026-10-21

Owner decisions: Nature Essentials licence confirmed as CC BY 4.0 (Gate 1); Rock029 (ambientCG)
as a rock layer over a sand base; `red_sand` (Poly Haven) chosen over `aerial_sand` from a
brightness-matched A/B (`Saved/Screenshots/DesertGround/ab_sheet.png`). Applied in the
DebugGame editor (UE 5.8.3, Linux, one editor):

- **Ground (rows 3–4):** new public `/OpenGolfSavanna/Materials/M_SavannaGround`
  (`BuildSavannaGround.py`). It has a red-sand base with seam-free noise patches of Rock029,
  and the Leafy Grass rough near play through the course mask. Both textures are scaled to
  target colours.
  - The private ground instances were reparented with every override cleared
    (`ApplyOpenGround.py`, `BuildPrivateRangeMaterials.configure_ground`), so they no longer
    reference MW.
  - Close-view sand renders (147, 97, 59) against MW's (150, 106, 70).
  - Poly Haven's 16-bit PNG sands imported washed out, close to white; the sands use 8-bit
    JPGs instead.
- **Nature Essentials (rows 5–7):** imported into `/OpenGolfSavanna`, in the `NatureEssentials`
  folders under Meshes, Materials and Textures: 18 meshes and 10 materials.
  - Uses the public `ImportNatureEssentials.py` and `blender/ExportNatureEssentials.py`; the
    pack path comes from `UOPENGOLF_NE_PACK` or `--source`.
  - Cards and rocks use `M_SavannaCard` and `M_SavannaSolid`; the grass uses the new
    `M_SavannaUnderstoryGrass` (row 6), which is dry unless a course enables its mask.
  - The private understory was re-placed on the public meshes: 18,185 pieces around 296 trees,
    the same count. Backup: `Saved/TerrainBackups/20261021-231500-understory`.
  - The 71 private copies were deleted after a referencer check.
  - The credit, changes note and full CC BY 4.0 text (`LICENSES/CC-BY-4.0.txt`) are in
    `OpenGolfSavanna/NOTICE.md` and `THIRD_PARTY_NOTICES.md`.
- **Gate 2:** none of the 201 public packages (`/OpenGolfSavanna`, `/OpenGolfCourse`, `/Game`)
  references private or restricted content.
- **Gate 5, added:** restricted packs remain in Git history and block any public release: Project
  Nature (626 files) on `main` and every branch, and DZ and MW in `4898a93`. See
  [the roadmap](AssetLicenseRoadmap.md#gates).

Incident: a command helper sent Ctrl+A and BackSpace to the level editor, which ran
`ACTOR SELECT ALL` and `DELETE` (121 actors). The saved map was never written; `TRANSACTION
UNDO` restored the actors only partly, so the map was reloaded from disk without saving. The
helper now sets text directly (`FillForm`).

Not yet verified: shimmer sequences and slow-motion review of the new ground and understory,
frame time, and owner parity sign-off for rows 3–7. Unused private leftovers (old oak and pine
presets, 160 orphaned PCG graphs, `M_PrivateDesertGrass`, `SM_PrivateGrassA–D`) still reference
DZ or MW and go when the private range retires.

## Savanna range on the public map — 2026-10-21

The owner chose to port the build pipeline rather than copy the private map. Applied to
`/Game/UOpenGolf/Maps/UOpenGolf` in the DebugGame editor (UE 5.8.3, Linux, one editor, rebuilt
with the new `UOpenGolfTerrainBuilder::AddVegetationHost`):

- **`BuildSavannaRange.py`:**
  - copied `DA_RangeCoastal` to `/Game/UOpenGolf/Terrain/Savanna/DA_SavannaRange` and moved the
    bunker onto the approach
  - set the Grass008 fairway and green, the Ground093C bunker and the `M_SavannaGround`
    rough/outer ground
  - rebuilt the terrain and the 36 seeded jacaranda patches
  - results:
    - 296 trees and 296 wood colliders; 592 of 592 wood hits clear
    - 18,185 understory pieces, the same counts as the private map
  - backup: `Saved/TerrainBackups/20261022-022823-savanna-range`
- **`ApplyCourseGrass.py`:** 15,006 course-grass pieces, as on the private map. Backup:
  `Saved/TerrainBackups/20261022-022851-422080-course-grass`.
- **Captures:** the same poses on the public map (tee, edge, close, bunker, green, close turf,
  wide trees) match the private map's after-state.

- **Gate 2:** none of the 327 public packages references private or restricted content.
- **Tests (in the editor, through MCP):** 22 of 25 `UOpenGolf.Range.*` and
  `UOpenGolf.Course.Terrain.*` tests pass; `FrameRates` was not run. The three failures assert
  the old flat-box range:
  - `EditableLayout`: more than 20 layout pieces, and rough under fixed points
  - `Presentation`: the tee uses `M_Turf`
  - `RolloutSurfaces`: lies at flat-layout offsets, and a 121.6-yard replay shot that now lands
    in the moved bunker and stops

  The flat boxes were already absent from the public map before this conversion (the 10-19
  backups have them; today's pre-run backup does not). Only the bunker landing is new. The owner chose
  to leave them failing; they are recorded as technical debt in the [roadmap](Roadmap.md#technical-debt).

Not yet verified:
- shimmer sequences, slow-motion review and frame time
- owner parity sign-off for rows 1–7

`SM_Jacaranda` was left marked unsaved in the editor after the run, with its settings unchanged
(Nanite on, no shape preservation); it was not saved. Cause, found later the same day: the old
`tree_wood_collision.proxy_mesh` wrote metadata tags onto the visible tree on every run. The scripts neither add nor remove the 50–300-yard
markers; the owner removes them by hand.

## Tree collision: trunk and main limbs, canopy drag — 2026-10-21

The owner wanted ball collision on the trunk only, so twigs stop bouncing the ball back as hard as
the trunk. Owner decisions: the trunk and main limbs block; branches and leaves slow the ball
("canopy drag") rather than letting it pass untouched.

Measured in `<art root>/Jacaranda/Jacaranda.blend`:

| Material | Triangles | Height | Reach from trunk |
|---|---|---|---|
| `jacaranda_tree_trunk` | 28.7k | 0–10.4 m | up to 4.5 m |
| `jacaranda_tree_branches` | 221k | 2.7–19.4 m | median 6.6 m |
| `jacaranda_tree_leaves` | 554k | 2.5–19.6 m | median 7.4 m |

The trunk material is the trunk and its main limbs.

**Step 1: trunk-only collider (applied to the public map, saved).**
- **Export:** `blender/ExportJacaranda.py` now also exports two hidden helpers:
  - `SM_JacarandaCollision`: the trunk material's faces only, at the tree's transform
  - `SM_JacarandaCanopy`: a 64-point convex hull of the branches and leaves, with its `UCX_`
    collision, starting 2.55 m up
- **Import:** `ImportSavannaAssets.py --meshes=…` imported both without touching the tree. The
  collider is 1.15 MB (the old full-tree copy was 29.7 MB): 28,744 triangles, complex-as-simple,
  Nanite off. The canopy has one convex hull.
- **Apply:** `ApplyTreeCollision.py` swapped all 296 colliders and checked them:
  - 296 of 296 trunk sweeps hit
  - 888 of 888 sweeps through a branch face, a leaf face and an open gap stayed clear
  - 0 failures
  - backup: `Saved/TerrainBackups/20261022-030713-tree-collision`
  - an earlier attempt failed before saving (its backup is `…/20261022-030625-tree-collision`)
- **Python tests:** `export_rules` (34) and the savanna script tests (27) pass.

**Step 2: canopy drag (C++; applied to the public map after the owner's DebugGame rebuild).**
- **OpenFairwayPhysics:**
  - a ball in the air inside a component tagged `OpenFairway.Canopy` (overlap-only) loses speed
    as `exp(-path / 4 m)`, with spin scaled the same way
  - it turns by a seeded angle of at most `6°·√path`, seeded by tree instance and step, so a
    repeated shot takes the same path
  - one overlap query per airborne step
- **OpenGolfCourse:**
  - `UOpenGolfVegetationPreset::TreeCanopyMesh` places hidden `OverlapAll` canopy volumes on a
    separate `OpenGolf.Terrain.TreeCanopy` actor, keyed by tree mesh
  - `UOpenGolfTerrainBuilder::RebuildTreeCanopy` replaces them on a baked course
- **Builds:** the Development build and the full unity build (`-DisableAdaptiveUnity`) pass.
- **Tests, in the editor through MCP:** 16 of 18 pass, including `Physics.CanopyResponse`,
  `Physics.ObstacleResponse`, `Course.Terrain.Definition` (with the canopy preset rule), all
  `Course.Terrain.*`, `Physics.Core`/`Fixtures`/`Wind`/`GreenRollout` and `Range.Shot` (open-air
  shots are unchanged). Two fail:
  - **`Range.ObstacleContact`, canopy case.** The test placed the canopy wrongly: its 4° drive
    carries only 20 m, so it crossed just 2.3 m of the 16–24 m box in the air (carry 17.87 m
    against 20.15 m). The trunk and flat-top cases pass. The box is now at 4–12 m; the
    Development build compiles, but the rerun waits for the next DebugGame rebuild.
  - **`Physics.Sand`, unrelated and already open.** It is the headless sand-plow model from
    2026-10-20 ("A ball rolling into sand at 4 m/s stops after 2.48 m", against 2 m). This change
    doesn't touch that code path; it is still to be tuned (`SandPlowRatio`).
- **`ApplyTreeCollision.py --canopy`:**
  - placed 296 canopy volumes
  - with them in place, 296 of 296 trunk sweeps hit and 888 of 888 branch, leaf and gap sweeps
    stay clear, so the canopy never blocks
  - map saved; backup `Saved/TerrainBackups/20261022-032327-tree-collision`
- **`PlayTreeShots.py` in Play on one tree, no wind:**
  - a 12 mph shot at the low trunk rebounds
  - a 40 mph shot at a mid-height trunk face rebounds, keeping 5% of its speed
  - a 40 mph shot across the outer crown doesn't rebound and keeps 58% of its speed after 1 s
    (free flight would gain speed as it falls)
- **Clean state:** no map or content package was left unsaved; `SM_Jacaranda` is no longer
  dirtied.

Not yet verified:
- the corrected `ObstacleContact` canopy case
- frame time with the canopy queries
- how the drag strength looks in Play (owner review; the constants are tunable)

## Project audit fixes — 2026-10-27

Linux, Unreal 5.8.3. The changes were first compile-checked with the Development editor build, then
tested through MCP in the owner's rebuilt DebugGame editor (see Tests below). No rendered check was run.

Changed:
- **Launch monitor:**
  - The transports post back through a weak reference set at creation. `AsWeak()` asserted on the
    worker when the transport was destroyed during a connect, which happens when Play stops or the
    game quits mid-connect.
  - A failed arm is retried twice, a ready delay apart, then the link is dropped so auto-reconnect
    re-handshakes. Three failed heartbeats in a row also drop it.
  - An identical unreadable frame after a re-arm re-arms again; readable repeats stay filtered.
  - Only one reconnect timer can be pending.
  - A failed BlueZ system-bus connection is reported once and no longer spins the worker.
  - A Windows read failure completes empty, as on Linux, instead of dropping the link.
- **Game:**
  - Play starts with two range controllers, using the first and logging an error.
  - A shot still moving after `MaxShotSeconds` (30 s) ends as a safety stop.
  - Re-teeing a moving ball finishes its result as interrupted.
- **Performance:**
  - The tee's ground trace is cached during Play.
  - The flag skips its solve and mesh upload when it has not been drawn for 0.5 s in Play.
  - The terrain grass switch is applied only when it changes.
  - HUD text is rebuilt at 10 Hz, while the wind arrow still updates every frame.
  - The canopy query reuses its buffer.
- **Robustness:**
  - Flag cloth: non-finite wind reads as calm, and a jumping pole carries its flag.
  - `RebuildVegetation` keeps the course grass layer.
  - Linux recordings fall back to a no-replace rename (`RENAME_NOREPLACE`) where the filesystem
    has no hard links.
- **Exporters:**
  - `png8.load` sets a texture's colour space before scaling or editing. Assigning it in
    `png8.save`, even to the same value, reloaded the file, so later `scale()` and `premultiply()`
    calls were silently lost; a Blender 5.0.1 probe confirmed both.
  - The jacaranda and Nature Essentials exporters now write 8-bit PNGs through `png8`.

Measured:
- **Builds:** the Development editor build and the full unity build (`-DisableAdaptiveUnity`) pass.
  - The Windows transport changes are not compiled on Linux.
- **Python:** `test_export_rules`, `test_ne_rules` and `test_grass_rules` pass (36 tests).
- **Jacaranda re-export (`ExportJacaranda.py`):**
  - 10 PNGs, all 8-bit, at the same sizes as before: 2K colour and normal maps, 1K masks.
  - Source size fell from 110.6 MB to 43.3 MB.
  - Against the old 16-bit export, trunk colour, normal and roughness differ by at most 1/255.
  - Leaf colour matches on opaque pixels (mean 0.15/255), but semi-transparent edge pixels are
    darker. The old near-white fringe (mean 0.83 at alpha 0.05–0.5) is now 0.49.
- **Nature Essentials re-export:** 40 PNGs, all 8-bit and at the old sizes; 90.7 MB fell to 82.3 MB.
- **Re-import:** none. The outputs are in `Saved/AssetImport`. The previous exports are kept
  outside the repository for comparison.
- **Tests, run later the same day** in the owner's rebuilt DebugGame editor through MCP. Only one editor was running.
  - **Non-map batch:** 21 of 23 pass, including `LaunchMonitor.TransportTeardown`, `.ArmRecovery`, `.HeartbeatLoss`,
    `.Session`, `.Cancellation`, `Course.FlagCloth`, `.VegetationRebuild` and the `Physics` core tests. Two new
    tests failed and were then fixed:
    - `Range.ShotRecordingFinalize` caught a real bug. The fallback used `IFileManager::Move` with replace off,
      but on Linux that still renames over an existing record. It now uses `renameat2(RENAME_NOREPLACE)`.
    - `Course.FlagClothInputs`: the test's "calm" flag kept the 4.47 m/s wind set earlier in the test,
      because `Reset` keeps the wind. The test now sets calm air and compares against a flag that never moved.
  - **Map and Play batch:** 10 of 16 pass, including `Range.DuplicateController`, `Physics.ShotInterrupt`,
    `Range.AutoReset`, `.WindSettings`, `.Shot`, `.ShotTrail`, `.PlayableBounds` and `.ReplaySelection`.
    - `EditableLayout`, `Presentation` and `RolloutSurfaces` are the known flat-range failures.
    - `Smoke.UOpenGolf` fails only because `Config/UserEngine.ini` opens the private map here.
    - `Range.TeeLocation` (new) could not move the range, because its root is Static in Play. The test now
      makes its throwaway Play copy Movable first.
  - **After a DebugGame rebuild:** `ShotRecordingFinalize`, `FlagClothInputs` and `TeeLocation` pass.
  - **`Range.Aim` fails only in the editor's first Play session after startup**, 2 of 2 such runs. It passed
    all 5 later runs: alone, after a fresh load of the public map, and before or after other tests.
    - The failure: one frame after a 30° turn, the tee camera is still exactly at its old spot and angle.
      Its 600 cm distance check passes, and the turn itself succeeded, so the ball counted as on the tee.
    - The frame pattern is the same in passing runs (Play starts in frame N, the check runs in N+1).
    - No change in this audit touches the camera, controller or tick code. Whether the failure predates the
      audit is not proven.
    - A diagnostic run (the third first-Play failure) showed the check ran 1 frame after the turn
      (world delta 0.077 s). The ball was still and 0.0 cm from the tee, and the camera yaw was exactly
      0.000. Any controller tick would have moved it: a snap to 30°, or about 0.13° even mid-return. So
      the controller did not tick between the two test frames. This is a test timing assumption, not a
      camera fault.
    - The test now waits for the camera's first update after the turn (at most 10 frames) and requires
      that update to be the full 30° turn on the aim line. A lagging camera would still fail.
      Why the controller skips that frame in the first session was not investigated further.
    - After a DebugGame rebuild, `Range.Aim` passed as the editor's first Play session (camera moved
      2 frames after the turn) and again in a later session (1 frame).

Not yet verified:
- **Hardware:**
  - quitting during a Square Home connect
  - link loss and recovery on Linux
  - the Windows build, connect and quit
- ~~Re-importing the jacaranda and Nature Essentials textures~~: done the same day; see
  [Texture re-import](#texture-re-import-8-bit-exports--2026-10-27). The owner kept the new look.
- **The committed Poly Haven grass textures were exported through the old `png8.save`.** Their
  albedo is not premultiplied, and `grass_medium_01_alpha_2k` stayed at 2K against the 1K opacity
  limit. A re-export would now premultiply and scale as `ExportPolyHavenGrass.py` intends, and needs
  the shimmer protocol.
- **Frame-time effect** of the flag, tee and HUD changes; no comparable timing was taken.

## Texture re-import (8-bit exports) — 2026-10-27

The jacaranda (10 textures) and Nature Essentials (40 textures) were re-imported in place from the
8-bit exports in `Saved/AssetImport`, with the new `--textures-only` option of
`ImportSavannaAssets.py` and `ImportNatureEssentials.py`. Only texture assets changed; material
instances (and their tuned values) and meshes were not touched. The re-exported Poly Haven grass was
not imported.

- **Size:** jacaranda assets 111.5 MB → 38.8 MB; Nature Essentials 84.6 MB → 69.8 MB.
- **Look:** much darker trees. The jacaranda trunk changed from pale grey to dark brown and the leaves
  to a deeper green without the near-white edges. The old 16-bit exports were the washed-out ones.
  The owner reviewed the captures and kept the new look, including the close-canopy shimmer increase.

Unreal Engine 5.8.3, Linux DebugGame editor, one editor via MCP, public map. Stationary
`UOpenGolf.Visual.CoastalTerrain` sequences, 20 frames per view at 1320 × 959 (FOV 65), measured with
`Scripts/grass_stability.py --profile coastal`. Pixels changing by more than 3/255, averaged over
consecutive pairs:

| View | Lower crop before | Lower crop after | Worst band before | Worst band after |
| --- | ---: | ---: | ---: | ---: |
| Tee, grass on | 0.002% | 0.002% | 10.343% | 9.225% |
| Tee, grass off | 0.002% | 0.002% | 10.320% | 9.724% |
| Edge, grass on | 0.012% | 0.012% | 10.425% | 10.390% |
| Edge, grass off | 0.003% | 0.003% | 10.415% | 9.863% |
| Close, grass on | 2.418% | 2.186% | 7.489% | 6.956% |
| Close, grass off | 0.011% | 0.010% | 6.751% | 6.572% |

The worst bands are the horizon and tree rows. These views show no regression.

**Jacaranda close-up** (not in the standard views): editor viewport, camera at (5700, −4200, 300),
pitch 15°, yaw 60°, FOV 90°, set once and then 10 captures. Setting the pose on every capture reset
the temporal history (17–19% change per frame), so it was not used.

| | Full frame | Canopy (top 60%) |
| --- | ---: | ---: |
| Before | 3.546% | 5.246% |
| After | 4.640% | 7.493% |

**The close canopy shimmers more after the change.** The darker leaves contrast more with the sky at
their cut-out edges, which may account for it; this was not verified.

> **Correction (later the same day):** do not rely on this close-up comparison. The same pose with the
> same assets measured 3.136% canopy change an hour later. Editor-viewport captures do not show the
> settled Play image: soft-shadow noise dominated them but made no difference in Play. Use the
> CoastalTerrain sequences, which run in Play. See [Tree canopy shimmer](#tree-canopy-shimmer--2026-10-27).

Evidence: `Saved/Automation/CoastalReview/BeforeTextureReimport-20261027-221111` and
`AfterTextureReimport-20261027-221740` (frames, `Stability.txt`, `TreeStability.txt`,
`tree_before_after.png`, `tree_leaves_zoom.png`, `edge_before_after.png`).

Not yet verified:
- slow camera motion: the test's motion sequence (View 9) was captured but not reviewed
- the Poly Haven grass re-export and import

## Git LFS migration — 2026-10-27

All `.uasset` and `.umap` files moved to Git LFS, including their history:
`git lfs migrate import --everything --include="*.uasset,*.umap"` (git-lfs 3.7.1). The 6 commits
held 376 `.uasset` and 3 `.umap` versions (543 MB).

- **Checks before the push:**
  - all 328 current assets are byte-identical to the pre-migration commit
  - the rewritten `main` merge matches the original file for file (584 files), with every pointer
    hash equal to the original asset's SHA-256
  - the only other change in each commit is the two LFS lines in `.gitattributes`
- **Push:** the owner force-pushed the rewritten `main` and `feature/cleanup-textures-re-use` with
  leases pinned to the original commits. The two merged branches were deleted instead of rewritten.
  GitHub's LFS batch API then reported all 378 objects referenced by `main` and the branch as
  present (537 MB).
- **Backup:** a bundle of the pre-migration history, kept outside the repository.
- **Working copy:** the migration's checkout left LFS pointer files in place of the assets, while the
  editor was running. `git lfs checkout` restored them before the editor touched them. On another
  machine, run `git lfs pull` if assets show up as small text files.
- `--everything` rewrites commits reachable from remote refs but does not move the remote refs.
  The rewritten `main` merge was found among the unreferenced commits and pushed by its hash.

Still to do:
- re-clone the Windows checkout with Git LFS
- install Git LFS on both self-hosted runners and run the Package workflow once
- check the account's GitHub LFS storage and bandwidth limits; each package job downloads about 340 MB

## Tree canopy shimmer — 2026-10-27

Unreal Engine 5.8.3, Linux DebugGame editor, one editor via MCP, public map.
`UOpenGolf.Visual.CoastalTerrain` sequences in Play: 20 frames per view, 0.1 s apart, FOV 65.

**Where it shimmers.** Only in the jacaranda canopies, plus a few cut-out understory cards near the
trees. Turf, greens, sand and bare ground are stable in every view: the lower-crop figures of
`grass_stability.py` stayed at 0.001–0.015% (2.2–2.4% with close grass on) in every run below. In the
canopies about 19% of pixels change by more than 3/255 between captures. The change is small (2.2/255
on average), and 0.7% change by more than 16/255, which reads as sparkle. It is random from capture to
capture rather than periodic. Leaves and twigs smaller than a pixel at mid and long range do not
settle under TSR.

**Experiments.** Each was one full sequence. Every one except Preserve Area was a temporary editor
setting, reverted afterwards. The tree region is a 300 × 180 crop of the edge view's tree row.

| Run (evidence folder) | Tee worst band | Edge worst band | Tree region > 3/255 | > 16/255 |
| --- | ---: | ---: | ---: | ---: |
| Baseline (`BeforeShimmerFix-20261027-232405`) | 10.46% | 10.71% | 19.36% | 0.691% |
| Leaf opacity mips keep coverage at the 0.45 clip (`LeafCoverage045-…`) | 9.59% | 10.02% | 17.85% | 0.615% |
| …plus `r.TSR.ShadingRejection.Flickering.Period 4` (`LeafCoverageTSRPeriod4-…`) | 9.44% | 9.95% | — | — |
| Directional soft-shadow rays off (`HardSunShadows-…`) | 10.66% | 11.01% | 20.10% | 0.691% |
| `r.TSR.History.ScreenPercentage 200` (`TSRHistory200-…`) | 14.26% | 13.89% | 26.12% | — |
| `SM_Jacaranda` Nanite Preserve Area (`PreserveArea-20261027-234941`) | 14.95% | 12.40% | 20.28% | 0.765% |

- Lumen GI is already off in project config (`r.DynamicGlobalIlluminationMethod=0`).
- In editor-viewport captures, turning off the soft-shadow rays cut the canopy change from 14% to 2%,
  but in Play it made no difference. Editor-viewport captures are not a valid shimmer measure.
- **Preserve Area** (saved on `SM_Jacaranda`; `ImportSavannaAssets.py` now sets it for `tree`
  meshes) fills the distant crowns: they read as trees in leaf instead of bare twigs (see
  `before_after_crowns.png` in its folder). It does not reduce shimmer per canopy pixel. The worst
  bands rise because more of each band is canopy.
- The Preserve Area run was captured at 1261 × 959 instead of 1320 × 959, because the editor layout
  changed when the Static Mesh editor opened. The framing differs by about 5%.

The owner kept Preserve Area (fuller crowns). The cause and fix are below.

### Cause and fix (later the same day)

Same editor and settings. Every run was captured at 1261 × 959 (the last at 1260 × 959). The tree
canopy boxes are now printed by `Scripts/grass_stability.py --profile coastal`. Each box is a fixed
fraction of the frame, so its figure does not grow with canopy area:
- tee (0, 0.38, 1, 0.52)
- green (0.45, 0.16, 0.95, 0.34)
- edge (0.38, 0.19, 0.75, 0.40)
- bunker (0, 0, 1, 0.18)

"Changed" counts pixels that change by more than 3/255; "sparkle" counts those above 16/255.
Evidence folders are under `Saved/Automation/CoastalReview/`, named after each run.

| Run | Tee changed / sparkle | Green | Edge | Bunker | Close grass (turf crop) |
| --- | ---: | ---: | ---: | ---: | ---: |
| R1 baseline | 8.36% / 0.267% | 10.79% / 0.137% | 18.61% / 0.707% | 9.16% / 0.265% | 2.27% |
| R4 `r.TSR.ThinGeometryDetection 1` | 7.21% / 0.030% | 8.66% / 0.022% | 15.73% / 0.074% | 7.66% / 0.051% | 1.42% |
| R5 R4 + `Coverage.ShadingRange 2` | 7.35% / 0.069% | 9.17% / 0.029% | 15.93% / 0.096% | 7.91% / 0.087% | 1.34% |
| R4c `r.TSR.History.SampleCount 32` only | 3.90% / 0.158% | 3.79% / 0.075% | 9.10% / 0.405% | 4.25% / 0.167% | 0.90% |
| R4b both (thin geometry + 32 samples) | 2.37% / 0.023% | 1.92% / 0.022% | 5.21% / 0.046% | 2.28% / 0.040% | 0.26% |
| **R9 final repeat of R4b** | **2.44% / 0.024%** | **1.81% / 0.018%** | **5.21% / 0.047%** | **2.24% / 0.039%** | **0.26%** |

**Why it shimmered.** There are two causes, both in how the sub-pixel leaves are rendered over time:
1. **Sparkle: TSR throws the leaves' history away.**
   - TSR's debug grid (`r.TSR.Visualize -2`, run R2) shows canopy pixels scattered with freshly reset
     history.
   - TSR's anti-flicker heuristic is disabled wherever sky shows through the leaves. That is why a
     longer flicker window did nothing.
   - Thin geometry detection is built for this case. It is off by default unless Nanite Foliage is
     enabled. Turning it on removes 81–90% of the sparkle.
   - Full relaxation for every material (R5) is no better, so the leaves' shading model does not need
     to change.
2. **Faint boiling: per-frame shadow noise on the leaves.**
   - With temporal AA off (R3), the ground is perfectly still but leaf pixels still change every frame.
   - Turning off SSAO changed nothing.
   - Turning off dynamic shadows made the canopy perfectly still. So did turning off only the virtual
     shadow map's screen-space trace (`r.Shadow.Virtual.ScreenRayLength 0`) together with soft-shadow
     rays. Both draw new noise every frame.
   - With TSR on, removing the screen trace alone gave nothing (R8 ≈ R4), because TSR already averages
     that noise. A longer history (32 samples instead of 16) averages it better.

**Fix.** `Config/DefaultEngine.ini` now sets `r.TSR.ThinGeometryDetection=1` and
`r.TSR.History.SampleCount=32`. No asset changed.

Against R1:
- Canopy change falls 71–83% and sparkle 85–93% in all four tree views (R9).
- The turf crop gets steadier as well: close grass 2.27% → 0.26%; stationary views 0.001–0.003%.
- Mean canopy colour moved by less than 0.35/255, so the look is unchanged.

**Checks for side effects:**
- **Ghosting:** `Visual.ShotTrail` (ball and trail in flight with a following camera) and the
  `Visual.RangeAssets` flag at 10 mph were compared before and after at full resolution. There is no
  smearing on the trail, flag edges or trees.
  - Before: ShotTrail `20261027-174338`, flag `20261027-174558-FlagWind10mph.png`.
  - After: ShotTrail `20261027-174616`, flag `20261027-174830-FlagWind10mph.png`.
  - `Visual.RangeAssets` fails its old flat-range check that the tee uses `M_Turf` (it is
    `M_CoastalSurface`); its captures were still written.
- **Cost:** mean frame time in ShotTrail was 10.39–10.59 ms after, against 10.35–10.54 ms before (DebugGame
  editor, 1280 × 720 render; p95 sits at the ~8.3 ms frame cap). That is within noise; this is not a
  controlled FPS measurement.
- **Slow motion (View 9):** `View9_before_after.gif` in the R9 folder shows before (left) and after (right).
  - No smearing is visible in the frames.
  - A second-difference measure on the tree rows is unchanged (6.4% vs 6.2%). It is dominated by the
    motion itself, so it shows no regression but cannot show an improvement.
  - The owner's viewing of the GIF is pending.

Not verified: Windows (DX12), packaged builds, and the owner's motion review.

## Distance markers removed — 2026-10-27

The owner decided the range does not need distance markers.
- `AUOpenGolfRange` no longer builds the 50–300-yard posts and number labels, so a new or empty
  layout does not bring them back. Their terrain alignment and colouring code is gone too.
- The 24 marker actors were deleted from the public map and the private map. The tee box, the only
  other decoration, moved from the `Range/Markers` outliner folder to `Range/Tee`.
- Checks:
  - The Development editor build compiles.
  - On the updated public map, `Range.Shot`, `.TeeLocation`, `.Aim`, `.AutoReset`,
    `.PlayableBounds` and `.WindSettings` pass. They ran in the DebugGame editor before its rebuild,
    so with the old code, which builds a layout only when the map has none.
- Not yet run: the same tests after the owner's DebugGame rebuild.

## Maintained turf preparation — 2026-10-27

The owner approved original fairway, first-cut, collar and green assets, with a 1 m visual
first cut, nearby blade detail and unchanged physical surfaces. Blender MCP 2.2.4 is installed
at user level for Codex and Claude Code. Live scene inspection and viewport capture worked.

The original blade exporter and matching texture baker are prepared. New native preset fields
select a distance reference surface and keep tall rough outside the fairway band. DebugGame
builds succeeded; native automation execution and editor application remain pending a restart.
Python blade footprint/repeatability and mask boundary/region-order checks pass (four cases).
No C# files were changed.

Before captures: `Saved/Automation/CoastalReview/Review-20261028-013205`, Unreal 5.8.3 DebugGame,
Linux, one interactive editor, 1261 x 959 Play captures, 20 frames per view. Analysis:
`Saved/Automation/CutTurf/before-stability.txt` with `grass_stability.py --profile coastal`.

| View | Lower-frame changed pixels | Worst horizontal band |
| --- | ---: | ---: |
| Tee, grass on | 0.001% | 4.083% |
| Tee, grass off | 0.001% | 4.171% |
| Edge, grass on | 0.003% | 3.349% |
| Edge, grass off | 0.000% | 3.385% |
| Close, grass on | 0.262% | 2.896% |
| Close, grass off | 0.002% | 2.798% |

These are before-only results, not evidence for the new turf. Final asset imports, repeated
PCG bakes, terrain/rollout tests, after-change shimmer, slow motion review, and a controlled
frame-time comparison are pending. Existing close rough already exceeds the 0.05% lower-frame
target. Preserve the editor layout for a comparable after run. The public map is unchanged
at this preparation checkpoint; no owner visual acceptance is claimed.

Preparation update: 60 original blade meshes and all four families' 2K colour, normal and
roughness bakes completed through Blender MCP. The mask is cropped around fairways rather
than the full detailed terrain. Native import/apply and all after-change evidence remain
pending the editor restart.

## Maintained turf: fuller Blender revision — 2026-10-27

The owner rejected the first turf pass as sparse and too regular. Its application and tests
are historical evidence, not visual approval. The Blender source now uses loose clumps,
scattered filler, curved blades and variation in width, height, bend, twist and colour.
Nominal fairway/first-cut/collar heights remain 15/30/20 mm.

The revised editable source and explicit previous-version backup live in the external art
root under `CutTurf/`; source paths, recovery steps and commands are in
[Blender grass handoff](TurfWorkflow.md). The source has 60 mesh exports and four sets of 2K
colour, normal and roughness bakes. In a 12.5 cm source patch, seed 1731 measures about 75%
projected fairway coverage and 85,000 blades/m². This is not a runtime performance figure.
The three source tests pass, including coverage checks for four variants of all mesh families.

The revised art was reimported with the scoped refresh path, preserving surface tuning and
placement. `Saved/Automation/CutTurf/fullness-import-status.json` records success, and the editor
logged `OPEN_GOLF_CUT_TURF_REFRESH_COMPLETE`. The complete first-pass Unreal art backup is
`Saved/TurfAssetBackups/20261028-024936-before-fullness/`. The `025148` retry backup already
contains revised textures and must not be treated as a complete first-pass rollback.
Blender before/after previews are in `Saved/Automation/CutTurf/Fullness/`.

Final rendered inspection, shimmer sequences, slow-motion/fade review, frame-time comparison
and owner acceptance for this revision remain pending. None of the previous measurements
establishes stability or performance of the denser grass. `CLAUDE.md`, `CODEX.md` and `AGENTS.md`
now point agents to the shared handoff; machine-specific details remain in ignored local notes.

## Maintained turf: shared field, smooth edges and square tee — 2026-10-27

The owner reported a different-coloured disc of grass around the camera, jagged fairway and
first-cut edges, and a square tee inside a circle. Causes found in the previous revision:

- The live blade material was an older graph: flat `TargetColor` with ±16% per-blade tone and no
  ground texture. The ground used its texture with `TextureMean` set to the target colour, so it
  rendered about 5% darker than the blades.
- Blades stopped at the 15–20 m instance cull. Their distance sink used world position offset,
  which the turf instances never evaluate.
- The fairway outline had 17 corners. The only blend read an 8-bit mask with
  18.5 × 4.9 cm texels, and collar, green and bunker edges had none. Tile grass stepped in
  6.25 cm squares.
- A painted 5 × 5 m tee box sat inside a 3 m round grass clearance.

Changes (owner-approved plan, decisions: near-only matched blades, square tee, smoothed
physical outlines; rough↔savanna sand out of scope):

- The tee is a square everywhere: `LevelTeeHalfSizeM` (default 2.5 × 2.5 m) replaces
  `LevelTeeRadiusM`, so the ground is levelled and grass cleared to the square.
  `AUOpenGolfRange::bShowPaintedTeeBox` is off on the public map.
- `UOpenGolfTerrainBuilder::RebuildSurfaceMeshes` swaps only the generated ground meshes.
  `BuildTerrain` would destroy every generated actor, including trees and vegetation.
- The outlines pass through their authored corners as centripetal Catmull-Rom curves
  (`outline_smoothing.py`). Fairway: 17→789 points; fringe, green and bunker:
  48→212/174/112. The generator `Scripts/UpgradeRangeTerrain.py` uses the same smoothing.
- `T_CourseSurfaceField` (4096 × 1024 RGBA8) stores signed distances to fairway, green,
  fringe and sand over every play region.
  - Ground, rough and blades share one world-space shader (`turf_field.py`): rough, first
    cut, fairway, bunker lip, collar, green and tee.
  - Mowing lines blend over 6 cm. The first cut's outer edge blends over 20 cm.
  - Stripes keep their footprint filter, and green stripes stay off.
- Blades:
  - Rebuilt lighter in Blender. Fairway and collar use one tapered quad, with first cut
    keeping two segments.
  - A 50 cm fairway tile drops from 85,512 to 21,430 triangles. Coverage checks still pass.
  - They draw to 10 m (first cut 14 m), with tone variation fading out between 4 and 7 m.
  - World position offset is removed from the material.
- `texture-means.json` records each colour bake's measured mean, so the ground's texture
  average equals its target colour.

Backups: `CutTurf-before-lighter-20261027-210610.blend` and bakes in the art root;
`Saved/AssetImport/OpenGolfSavanna/CutTurf-before-lighter-20261027-210610/`;
`Saved/TurfAssetBackups/20261028-042123-before-lighter-refresh/` (Unreal art, map, definition)
and `…-042659-before-turf-field/` (materials); `Saved/TerrainBackups/20261028-0422*` (map,
definition).

Applied in the owner's rebuilt DebugGame editor (Unreal 5.8.3, Linux, one editor), each step
confirmed in `LogPython`:
`OPEN_GOLF_CUT_TURF_REFRESH_COMPLETE`, `OPEN_GOLF_OUTLINES_SMOOTHED`, then
`OPEN_GOLF_COURSE_GRASS_COMPLETE course_grass=143573` (previously 142,955), and finally
`OPEN_GOLF_TURF_FIELD_COMPLETE`. Pixel-shader instructions: ground 763, blades 728, rough 977.

Tests (MCP, same editor):

- Pass:
  - `Course.Terrain.TiledGrass`, `.BoundaryGrass`, `.FirstCut`, `.Definition`, `.Scatter`
    and `.RandomScatter`.
  - `Course.SurfaceRebuild` (new) and `Course.VegetationRebuild`.
  - `Range.CoastalTerrain`, `Range.TeeLocation` and `Range.Presentation`.
  - `Physics.GreenStimp`, `Physics.GreenRollout` and `Visual.CoastalTerrain`.
- `Course.Terrain.TeeBox` (new): levelling, signed distance, clearance, inside checks and
  validation pass. Its "grass reaches the edge" checks failed because the test left the
  preset's default 0.5 m clearance; the cut-turf presets use 0. The test now sets 0 and
  compiles, but needs a DebugGame rebuild to re-run.
- `Range.EditableLayout` fails only its known flat-range assertions. `bShowPaintedTeeBox` is
  saved off.
- Python: outline 8/8, course field 8/8, all course, savanna and Blender script suites
  pass (13/35/42).

Rendered evidence: `Saved/Automation/CoastalReview/AfterTurfField-20261028-042858` against
`BeforeEdgeRepair-20261028-032124`, using the same test, editor layout and 1261 × 959 Play
captures.

| Measure | Before | After |
| --- | ---: | ---: |
| Tee view, grass on − off, mean (lower half, /255) | 2.08 | 0.45 |
| Tee, grass on − off, column at 6–9 m (RGB) | +4.4/+5.2/+2.3 | 0/0/0 |
| Tee grass on, stationary mean max diff (/255) | 0.379 | 0.291 |
| Close grass on, stationary mean max diff (/255) | 0.560 | 0.461 |
| Close grass on, changed pixels | 0.145% | 0.071% |
| Slow pan (View 9), second-difference flicker > 16/255 | 15.6% | 8.4% |

Canopy boxes are unchanged (sparkle 0.019–0.045%). Remaining pan flicker is in the tall rough
and trees, outside this change. Clips: `Saved/Automation/CutTurf/turf-field-motion-before.mp4`
and `-after.mp4`.

Limits: frame time was not re-measured. The earlier probe read 8.3 ms with grass both on and
off, which looks frame-capped, so it cannot show the difference. A GPU profile remains pending.
The previous `SM_Surface_*` build folder is no longer used by the ground actors. `T_FirstCutDistance`
is no longer wired to any output, but disconnected nodes in the old graphs may still reference it.
Cleaning up both awaits the owner. Owner visual acceptance is pending.

## Maintained turf relief and palette — 2026-10-27

Applied to the public map in one Unreal 5.8.3 DebugGame editor on Linux/Vulkan SM6.
The first cut now samples baked relief instead of flattening the ground normal. Maintained
surface relief follows terrain slopes and fades from 4–18 m. Blender MCP rebaked all four
existing families at 2K with stronger filtered normals and 0.92–0.96 roughness; meshes and
placement are unchanged. Fine colour texture has more weight, broad patches less, and the
mid greens are less saturated. Tall rough parent and instance match their before backups
byte for byte. Green stripes remain off; physics and lighting were not edited.

The original source and bakes are preserved under the external art root in
`CutTurf-before-relief-20261028`. Unreal textures and materials are preserved in
`Saved/TurfAssetBackups/Textures-before-relief-20261028` and
`Materials-before-relief-20261028`. The maintained-only application option and generator
changes preserve this result on reapplication. Import completion, field completion and
`OPEN_GOLF_TURF_RELIEF_TUNED` were confirmed in the editor log.

Validation after the final refinement:

- Python: savanna 45/45, Blender 42/42, course 13/13; changed builders pass syntax checks.
- `UOpenGolf.Range.CoastalTerrain` and `UOpenGolf.Visual.CoastalTerrain` pass.
- Before: `Saved/Automation/CoastalReview/AfterPalette3-20261028-052656` (fresh baseline,
  despite its inherited label). Final: `FinalTurfRelief-20261028-053458` in the same folder.
  Both are 200 Play frames at 1261 × 959 with the same layout and AA settings.
- `Saved/Automation/CutTurf/relief-before-stability.txt`, `relief-final-stability.txt` and
  `relief-comparison.json` contain measurements. The intermediate relief capture is not
  the final palette result.

| Measure | Before | Final |
| --- | ---: | ---: |
| Tee stationary mean max difference (/255) | 0.2918 | 0.3071 |
| Edge stationary mean max difference (/255) | 0.2940 | 0.2936 |
| Close stationary changed pixels (>3/255) | 0.116% | 0.113% |
| Tee worst horizontal band changed pixels | 4.068% | 4.200% |
| Edge worst horizontal band changed pixels | 3.249% | 3.372% |
| Close worst horizontal band changed pixels | 2.928% | 2.911% |
| Slow pan second difference (>16/255, whole frame) | 8.016% | 8.208% |

Final canopy-box sparkle is 0.016–0.042%. Tee grass-on/off lower-frame mean changes by
0.039/255; close grass-on/off remains about 4.26/255, including the retained tall rough.
The pan measurement includes trees and rough; it does not establish that all motion shimmer
is fixed. Stationary results are close to baseline, but repeated runs have not established
a noise interval for the small increases. Controlled uncapped GPU/frame-time comparison and
owner visual acceptance remain pending. No performance or visual-parity claim is made.
Editor background CPU throttling was temporarily disabled for automation and restored.
