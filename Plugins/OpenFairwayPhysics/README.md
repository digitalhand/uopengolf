# OpenFairway Physics

Native C++ golf ball physics for Unreal Engine 5.8: flight, impact, bounce, roll and
rest from [ogolf-physics](https://github.com/digitalhand/ogolf-physics), plus an Unreal
ball actor that uses Chaos scene queries for contact. No .NET runtime and no Godot.

Part of [uopengolf](../../README.md). Original code is MIT; ogolf-physics is the
maintainer's own MIT project, so the port is covered by this repository's license.

## Table of contents

- [Status](#status)
- [Features](#features)
- [Requirements](#requirements)
- [Enabling the plugin](#enabling-the-plugin)
- [Quick start](#quick-start)
  - [Blueprint](#blueprint)
  - [C++ ball actor](#c-ball-actor)
  - [C++ headless simulation](#c-headless-simulation)
- [How it uses Unreal physics](#how-it-uses-unreal-physics)
- [Surfaces](#surfaces)
- [Ball profile and launch regimes](#ball-profile-and-launch-regimes)
  - [Decoding a regime key](#decoding-a-regime-key)
  - [Lookup and continuous calibration](#lookup-and-continuous-calibration)
  - [Landing buckets](#landing-buckets)
  - [FlightScope comparisons and replay shots](#flightscope-comparisons-and-replay-shots)
  - [Does the regime limit rollout?](#does-the-regime-limit-rollout)
- [Flight corrections](#flight-corrections)
- [Shot results](#shot-results)
- [Units and coordinate frames](#units-and-coordinate-frames)
- [Tests and golden fixtures](#tests-and-golden-fixtures)
  - [Regenerating fixtures](#regenerating-fixtures)
  - [FlightScope shot library](#flightscope-shot-library)
- [Differences from ogolf-physics](#differences-from-ogolf-physics)
- [Known gaps](#known-gaps)
- [Plugin structure](#plugin-structure)
- [References](#references)
- [License](#license)

## Status

| Area | State |
| --- | --- |
| Core equations (flight, impact, ground, rest) | Ported, then corrected for spin decay, calibration continuity, excess low-launch lift, the high-Re drag step and the low-Re lift dead band; historical profile matches 21 C# fixtures |
| Air | Temperature, altitude, humidity, station pressure and wind |
| Headless simulator (`ShotSimulator`) | Ported from `PhysicsAdapter`; flat ground, one surface |
| Ball actor (`AOpenFairwayBall`) | Working on Windows; automated range shot within tolerance of the headless result |
| Surfaces | All seven presets; assigned by tags |
| JSON ball profiles, surface zones, terrain probes | Not ported yet (see [Known gaps](#known-gaps)) |
| Linux | Editor build and `UOpenGolf.Physics` tests pass (Unreal 5.8.3, 2026-10-16) |

Ported from ogolf-physics revision `e6d793ae9477efaf55dc9078d7260f76bb532a7d`.

## Features

- Aerodynamic drag and Magnus lift from inherited polynomial fits, with calibrated
  spin-drag, high-launch and mid-spin corrections. The legacy low-launch lift boost
  is neutral in gameplay; the historical profile retains it.
- Drag eases into its high-Re cap from Re 175k instead of stepping up 7.8% at 200k, and
  lift bridges the Re 55k-68k band where two inherited fits fall to zero (see
  [Flight corrections](#flight-corrections)).
- Air density from temperature, altitude or a measured station pressure, and humidity.
  Wind acts on drag and lift through the ball's velocity relative to the air.
- Impact impulses with speed- and spin-dependent restitution, friction-limited tangential
  response and a first-impact "bite" chosen per landing bucket (run, release, check, spin-back).
- Ground roll with sliding friction until rolling, spin-scaled rolling resistance, grass drag,
  slope drive and slope resistance, and rest detection.
- Seven surface presets: fairway, soft fairway, rough, firm, green (set by stimp), sand, fringe.
- Calibrated launch-regime overrides from the ogolf-physics FS reference data.
- Launch-monitor spin parsing (back/side spin or total spin/axis); a putt with no measured
  spin leaves the face rolling.
- A headless simulator for predictions and regression tests, and a Blueprint-ready ball actor
  with landed and at-rest events.

## Requirements

- Unreal Engine 5.8 with C++ (the same toolchain as the [project](../../README.md#prerequisites)).
- Only the engine's `Core`, `CoreUObject` and `Engine` modules. The core
  (`Public/OpenFairway/`) uses `Core` only.
- Optional, for regenerating fixtures: the .NET 9 SDK and an
  [ogolf-physics](https://github.com/digitalhand/ogolf-physics) checkout next to this repository.

## Enabling the plugin

The plugin is enabled in `UOpenGolf.uproject`. To use it in another project, copy
`Plugins/OpenFairwayPhysics/` into that project's `Plugins/` folder, add
`{"Name": "OpenFairwayPhysics", "Enabled": true}` to its `.uproject`, and add
`"OpenFairwayPhysics"` to the dependent module's `Build.cs`.

## Quick start

### Blueprint

1. Place an **Open Fairway Ball** in a level above ground that blocks the **Visibility**
   channel. The ball settles onto the ground at Begin Play.
2. Tag ground components (or their actors) with a surface tag such as
   `OpenFairway.Surface.Green`; untagged ground uses the ball's **Default Surface**.
3. Make an **Open Fairway Launch**: ball speed (mph), vertical and horizontal launch (degrees),
   and the spin values you have, with their **Has** flags set.
4. Call **Launch**. Bind **On Ball Landed** and **On Ball At Rest**, or read
   **Get Shot Result** every frame for live values.

### C++ ball actor

```cpp
#include "OpenFairwayBall.h"

AOpenFairwayBall* Ball = World->SpawnActor<AOpenFairwayBall>(TeeLocation, FRotator::ZeroRotator);

FOpenFairwayLaunch Launch;
Launch.BallSpeedMph = 150.0f;
Launch.VerticalLaunchDeg = 12.5f;
Launch.HorizontalLaunchDeg = 1.5f;
Launch.bHasTotalSpin = true;
Launch.TotalSpinRpm = 2335.0f;
Launch.bHasSpinAxis = true;
Launch.SpinAxisDeg = -9.9f;
Ball->Launch(Launch);

Ball->OnBallAtRest.AddDynamic(this, &AMyRange::HandleBallAtRest);
```

`PlaceAt(Location)` stops the ball and sets it on the ground. `AimYawDeg` turns launches
clockwise from the world +X target line. The **OpenFairway|Air** properties set
`TemperatureF`, `AltitudeFt`, `RelativeHumidity` (0-1), `PressurePa` (0 derives it from
altitude), `WindSpeedMph` and `WindFromDeg`; they apply from the next launch.

### C++ headless simulation

```cpp
#include "OpenFairway/ShotSimulator.h"

OpenFairway::FLaunchData Launch;
Launch.SpeedMph = 114.46f;
Launch.VerticalLaunchDeg = 6.95f;
Launch.HorizontalLaunchDeg = -0.63f;
Launch.BackSpinRpm = 1931.9f;
Launch.SideSpinRpm = -47.8f;

const OpenFairway::FShotSimulationResult Result =
    OpenFairway::ShotSimulator::Simulate(Launch, OpenFairway::ESurface::Fairway);
// Result.CarryM, Result.TotalM, Result.LateralM, Result.ApexM, Result.HangTimeS,
// Result.LandingSpinRpm, Result.bAtRest

OpenFairway::FAirConditions Air;  // 75 F, sea level, dry and calm by default
Air.RelativeHumidity = 0.5f;
Air.WindSpeedMph = 10.0f;
Air.WindFromDeg = 0.0f;  // headwind
const auto IntoWind = OpenFairway::ShotSimulator::Simulate(
    Launch, OpenFairway::ESurface::Fairway, OpenFairway::FBallPhysicsProfile(), Air);
```

Check `bAtRest` before treating `TotalM` as final; false means the 30 s limit ran out.
The core functions (`GolfPhysics::StepFlight`, `ResolveImpact`, `StepGround`, `CanRest`)
can also be called directly with `FBallState` in SI units.

## How it uses Unreal physics

ogolf-physics runs its ball as a Godot `CharacterBody3D`: Jolt only answers *where is
the contact* (`MoveAndCollide` and ray casts), and OpenFairway decides *what happens*.
This plugin does the same with Chaos:

| Step (120 Hz) | ogolf-physics (Godot + Jolt) | This plugin (Unreal + Chaos) |
| --- | --- | --- |
| Integrate flight or ground step | `BallPhysics.IntegrateStep` | `GolfPhysics::StepFlight` / `StepGround` |
| Move and find contacts | `MoveAndCollide` with a 0.5 mm safe margin | `SweepSingleByChannel` with a sphere 0.5 mm smaller than the ball |
| Stay grounded while rolling | `RayCast3D` ground probe | `LineTraceSingleByChannel` ground probe |
| Bounce, roll, rest | OpenFairway core | Same equations, ported |
| Surface under the ball | Lie resolver callback | `OpenFairway.Surface.*` tags on the hit component or actor |

The ball is never a simulating Chaos rigid body, so Chaos cannot fight the calibrated
bounce and roll. Walls (normals flatter than about 45°) reflect the ball at 30% speed, as
in ogolf-physics. Steps run at 120 Hz with up to 12 substeps per frame. After a long
stall, excess accumulated time is discarded rather than allowing unbounded catch-up.
Sustained rates below about 10 FPS therefore slow simulation relative to wall time.
The game module's `UOpenGolf.Range.FrameRates` regression checks matching carry,
total and simulation duration at 30/60/144/5 FPS and with 250 ms hitches. This verifies
fixed-step outcomes, not real-time playback at every render rate.

The game's ribbon tracer is separate from this actor's debug tracer. Its buffer/mesh
optimizations do not change the timestep, equations or collision queries. See the
[game validation notes](../../Docs/PhasesStatus.md#recording-reliability-bluetooth-cancellation-and-tracer-cpu-work)
for CPU measurements and frame-time limitations.

## Surfaces

Assign a surface by adding a tag to the ground component or its actor. The ball checks
the component first, then the actor, then falls back to **Default Surface**.

| Surface | Tag | Kinetic friction | Rolling friction | Notes |
| --- | --- | --- | --- | --- |
| Fairway | `OpenFairway.Surface.Fairway` | 0.50 | 0.050 | Calibrated roll-out baseline |
| Soft fairway | `OpenFairway.Surface.FairwaySoft` | 0.56 | 0.070 | Wet or soft turf |
| Rough | `OpenFairway.Surface.Rough` | 0.62 | 0.095 | Grabs the ball |
| Firm | `OpenFairway.Surface.Firm` | 0.30 | 0.030 | Hard ground, long roll |
| Green | `OpenFairway.Surface.Green` | 0.58 | From stimp (10 ft) | Spin-back enabled; rolls at its stimp speed |
| Sand | `OpenFairway.Surface.Sand` | 0.65 | 0.30 | Soft: lower restitution and retention |
| Fringe | `OpenFairway.Surface.Fringe` | 0.56 | 0.0725 | Close-cut collar |

`AOpenFairwayBall::GetSurfaceTag(Surface)` returns the tag. Values come from
`SurfaceCatalog` in `Private/OpenFairway/Surfaces.cpp`.

## Ball profile and launch regimes

`AOpenFairwayBall::Profile` (`OpenFairway::FBallPhysicsProfile`) holds the calibrated
defaults: drag scale 1.01, green stimp 10 ft, the flight/bounce/rollout constants, the
landing responses and the launch-regime overrides. Current defaults interpolate
override values between speed/launch/spin anchors, preserving the most-specific
lookup and neutral fallback at each anchor. Landing responses are interpolated
over launch/spin too. The regime and landing bucket are descriptive categories;
logs identify the resolved override as `interpolated`. Changes apply from the next
launch. See [the calibration analysis](../../Docs/FlightScopeAnalysis.md) for anchors.

Flight spin uses `exp(-SpinDecayCoefficient * speed / radius * dt)` with SI units
and a default coefficient of `2e-5`. Both flight-step overloads use the decay rate
stored in `FFlightAerodynamicsSample`, including speeds below the lift/drag cutoff.
`ResolveScaleOverride` and `ResolveLandingResponse` now return values, since the
interpolated result does not belong to one table entry.

`FBallPhysicsProfile::HistoricalReference()` disables interpolation and restores
the old constant 0.2/s decay and 1.08 low-launch lift recovery for pinned C# verification. It is not the gameplay
default; the reference harness and golden values remain unchanged.

Air conditions come from the ball's `TemperatureF` (default 75 °F) and `AltitudeFt`
(default 0), the same defaults as the ogolf-physics headless simulator.

### Decoding a regime key

The format is `<family>-<speed bin>-<vertical-launch bin>-<total-spin bin>`.
These are launch-condition classifications, not the selected club. Horizontal
launch and spin-axis direction do not select a regime. The authoritative decoder
is `ShotRegimeKey` in [BallProfile.cpp](Source/OpenFairwayPhysics/Private/OpenFairway/BallProfile.cpp).

Families are evaluated in this order:

| Family | Meaning | Condition |
| --- | --- | --- |
| `C` | Chip / very low speed | Speed < 60 mph, regardless of launch |
| `D` | Driver / wood | Otherwise, speed > 110 mph **and** launch < 18° |
| `W` | High loft | Otherwise, launch > 30° |
| `I` | Iron / approach | Everything else |

Exactly 110 mph does not qualify for `D`; exactly 30° does not qualify for `W`.
All bin intervals below include their lower bound and exclude their upper bound.

| Speed bin | mph | Launch bin | Degrees | Spin bin | rpm |
| --- | --- | --- | --- | --- | --- |
| `S0` | < 60 | `V0` | < 10 | `P0` | < 2500 |
| `S1a` | 60–72 | `V1` | 10–18 | `P1` | 2500–4000 |
| `S1b` | 72–85 | `V2` | 18–25 | `P2` | 4000–5500 |
| `S2` | 85–105 | `V3` | 25–33 | `P3` | 5500–7500 |
| `S3` | 105–120 | `V4` | ≥ 33 | `P4` | ≥ 7500 |
| `S4` | ≥ 120 | | | | |

Examples from the screenshots:

- `I-S1b-V2-P2`: 75.7 mph, 20.78°, 4463 rpm — iron/approach family,
  72–85 mph, 18–25° launch, 4000–5500 rpm.
- `D-S3-V0-P0`: 114.5 mph, 6.95°, 1933 rpm — driver/wood family,
  105–120 mph, below 10° launch, below 2500 rpm.

### Lookup and continuous calibration

The inherited table has 32 overrides. A hard lookup tries the full key, then
family/speed/launch, then family/speed, then family. For example:
`I-S1b-V2-P2` → `I-S1b-V2` → `I-S1b` → `I`. The first match wins;
entries do not stack. No match means multipliers of 1 and angle offset of 0.
Global profile multipliers apply afterward (global drag defaults to 1.01).

With the default `bInterpolateCalibration = true`, this lookup happens at up to
eight neighboring anchor points, then their results are blended in three axes:

| Axis | Anchors |
| --- | --- |
| Speed, mph | 54, 66, 78.5, 95, 107.5, 115, 125 |
| Vertical launch, degrees | 5, 14, 21.5, 27.5, 31.5, 36.5 |
| Total spin, rpm | 1250, 3250, 4750, 6500, 8500 |

Outside the anchor extent, that axis clamps to its nearest endpoint. All seven
override fields blend: drag, lift, kinetic friction, rolling friction, grass
viscosity, critical-angle offset and spin-back angle boost. This removes jumps at
the old family and bin thresholds. `RegimeKey` still reports the actual launch's
category; `MatchedRegimeKey = interpolated` means there is no single winning row.
Interpolation is continuous, but its slopes can change at anchors. Continuity
does not guarantee physical accuracy or monotonic carry over every input range.

The low wood's descriptive `D-S3-V0-P0` has no direct override. Its neighboring
`D-S3-V1` row contributes about 20.1% of the blended value, with the remainder
neutral. At decoded packet precision this produces regime drag **1.008047**,
lift **0.997988**, and net drag **1.018128** including the global multiplier.
Before the low-launch correction, disabling regime calibration raised carry from
128.13 to 128.79 yd: the blend slightly reduced that overshoot.

Tune `RegimeScaleOverrides` in C++ and inspect the resolved scales, not just the
printed regime label. An edited row influences neighboring launches through the
anchors; custom narrow rows not sampled by any anchor will have no effect in the
continuous mode. Test both sides of affected boundaries and compare carry, apex,
hang time and landing together. The table was fitted around the older spin decay;
smooth interpolation does not independently recalibrate those values. JSON
profile loading and the sibling project's offline fitting tools are not ported.

### Landing buckets

Landing labels use **launch** spin and vertical angle, not the club or decayed
impact spin. They describe a separate first-impact response table:

| Bucket | Classification | Spin bite scale | Maximum spin-back, m/s |
| --- | --- | ---: | ---: |
| Run | `P0` or `P1` | 0 | 0 |
| Release | `P2` | 0.50 | 0 |
| Check | `P3`, or `P4` with `V0`/`V1` | 0.85 | 0 |
| Spinback | `P4` with `V2`/`V3`/`V4` | 1.00 | 1.20 |

For non-neutral responses, bite ramps in over 8–20 m/s impact speed. Current
defaults blend the four response parameters over the same launch/spin anchors;
the displayed bucket is descriptive. Actual contact still depends on impact
velocity, spin and surface. Neither this table nor surface friction can change
carry before first contact.

### FlightScope comparisons and replay shots

In the game's **Test shots [P]** dropdown, select either screenshot or a driver,
iron, wedge, chip, high flop, or recorded Square Home shot. **Hit selected shot**
launches it; **R** repeats the selection. Inputs and available FlightScope targets
appear in the panel. The nine presets belong to the game's launch-monitor replay
catalog, not to the physics plugin's runtime dependencies.

| Screenshot | Current carry | FlightScope carry | Difference |
| --- | ---: | ---: | ---: |
| 22:05, 75.7 mph / 20.78° / 4463 rpm | 85.94 yd | 87.6 yd | −1.66 yd |
| 23:05, 114.5 mph / 6.95° / 1933 rpm | 121.55 yd | 119.8 yd | +1.75 yd |

These compare different air settings, and the first screenshot also has opposite
HLA signs. The second shot previously carried **128.13 yd**. Gameplay now sets
`LowLaunchLiftRecoveryMax = 1.0` instead of 1.08, removing the extra empirical
boost while retaining ordinary Magnus lift and the corrected spin decay. This
produces **121.55 yd**, 15.03 ft apex and 2.741 s hang time, versus FlightScope's
119.8 yd / 14.6 ft / 2.8 s. The boost only operated below 9.5° launch, further
weighted by Reynolds number and spin ratio. The earlier 20.78° iron and other
shots at or above 9.5° are unaffected; low-launch shots can shorten.

The diagnosis isolated regime interpolation (−0.66 yd before this change), spin
decay (+4.96 yd with hard lookups), and the low-launch boost (+6.58 yd with the
corrected spin model). The mid-spin boost had no effect on this shot. No regime
rows, boundaries or global drag/lift multipliers were changed to fit the reference.
This is a targeted calibration correction, not proof that all low-launch shots
now match measured flight. Full inputs, limitations, rollout differences and experiments:
[FlightScope analysis](../../Docs/FlightScopeAnalysis.md).

### Does the regime limit rollout?

For the low wood `D-S3-V0-P0`, `override interpolated` identifies the blended
launch calibration; it is not a distance cap. The resolved kinetic-friction,
rolling-friction and grass-viscosity multipliers are all **1.0**, the critical-angle
offset is **0**, and the spin-back boost multiplier is **1.0**. Its interpolated
Run response has zero extra landing bite and zero allowed spin-back. Flight
calibration still affects landing speed, angle and spin indirectly.

The original range had a 30 m green centered at 150 yd, with a 2 m fringe. This wood
landed on fairway but crossed into that green during rollout. Surface settings change
as the ball travels. In particular, the modeled green's grass viscosity is
**0.011**, versus **0.0017** on fairway and **0.00245** on fringe. This coefficient
enters speed-dependent translational drag and spin damping. The green also uses
stimp-derived rolling resistance and excludes the fairway's spin multiplier from
that resistance. A 10 ft stimp check validates low-speed roll in this model;
it does not validate the whole high-speed post-impact response against FlightScope.

The corrected shot on a single flat fairway gives **37.69 yd roll / 159.24 yd total**,
whereas the mixed range reproduces **31.92 / 153.47 yd**. Replacing only the green
with fairway, keeping the fringe, gives **36.71 / 158.26 yd**. Thus the original
range loses about 5.77 yd versus all fairway; this is a surface-response effect,
confirmed with identical actor and geometry. FlightScope's
Medium setting gives **38.7 / 158.5 yd**, with a shorter carry of 119.8 yd. Compare
equivalent surfaces before changing regime calibration to compensate for the
range layout. See the controlled surface experiment in the
[analysis](../../Docs/FlightScopeAnalysis.md#rollout-surface-isolation).

The test range now assigns **Fairway to every ground mesh**, including the former
green/fringe and side strips, with matching fairway appearance. The low wood now
rolls 37.69 yd to 159.24 yd total in the scene and rests on Fairway. The ball still
reads each contacted mesh's surface tag; no surface is forced inside the physics
solver or replay path. Other surface presets remain available for scenes that
assign them. The rollout regression test explicitly recreates the old mixed
assignments in a disposable test world to preserve that comparison.

## Flight corrections

Two corrections to the inherited aerodynamic fits are on by default. Both were measured
against the FlightScope shot library before being adopted, and `HistoricalReference()`
turns both off, so the golden fixtures are unchanged.

| Correction | Profile field | Problem | Shot-library effect |
| --- | --- | --- | --- |
| High-Re drag blend | `Flight.HighReCdBlendStart = 175000` | Drag stepped from 0.186 to the 0.2 cap at Re 200k (about 162 mph at 75 F) | No shot launched below 141 mph moves; library unchanged (it has one shot at or above 150 mph) |
| Low-Re lift bridge | `Flight.bBridgeLowReLift = true` | The 60k and 65k lift fits clamp to zero for spin ratios 0.1-0.2, so lift vanished at about 43-54 mph | Carry mean error 3.78 to 3.70 yd overall over 175 compared shots, better in every carry band |

Wind and humidity are inputs rather than corrections: calm, dry air gives exactly the
previous results. Wind is uniform with height and has no gusts. Humidity uses the Tetens
saturation pressure, with the dry air contributing its partial pressure.

## Shot results

`FOpenFairwayShotResult` is updated every frame while the ball moves and is passed to
the events:

| Field | Meaning |
| --- | --- |
| `CarryM` | Downrange distance at the first landing |
| `TotalM` | Current downrange distance; the total at rest |
| `RollM` | `TotalM - CarryM` (negative after spin-back) |
| `LateralM` | Offline from the aim line, positive right |
| `ApexM` | Highest point above the launch point |
| `HangTimeS`, `LandingAngleDeg`, `LandingSpeedMps`, `LandingSurface` | First landing |
| `Surface` | Surface under the ball now (the lie at rest) |
| `LandingBucket` | Run, Release, Check or Spinback |

Distances are measured along the shot's horizontal launch direction, like ogolf-physics.

## Units and coordinate frames

- **Launch input:** mph, degrees and rpm with GSPro signs: positive HLA, spin axis and
  side spin are to the right.
- **Core:** SI units (m, m/s, rad/s) in the OpenFairway frame: +X downrange, +Y up,
  +Z right (right-handed, as in Godot).
- **Wind:** `WindFromDeg` is the direction the wind blows from, clockwise seen from above,
  measured from the +X target line: 0 is a headwind, 90 comes from the right and 180 is a
  tailwind. It is fixed to the world, so it does not turn with `AimYawDeg`.
- **Unreal:** centimeters with +X forward, +Y right, +Z up (left-handed). The ball actor
  converts positions and directions at its boundary; spin stays in the core frame, so
  cross products keep their physical meaning.

## Tests and golden fixtures

Run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Test -Filter UOpenGolf.Physics
```

```bash
bash Scripts/ue.sh test --filter UOpenGolf.Physics   # add --engine-root PATH if UE_ROOT is unset
```

| Test | What it checks |
| --- | --- |
| `UOpenGolf.Physics.Core` | Air properties (including humid air and station pressure), spin parsing, launch vectors, regime keys, landing buckets, friction, rest and rebound rules, green speed |
| `UOpenGolf.Physics.Fixtures` | 21 historical-profile shots against unchanged C# golden values: distances (0.1 yd), apex (0.1 ft), hang time (0.01 s) and landing bucket; current defaults also checked for finite, completed shots |
| `UOpenGolf.Physics.FlightCorrections` | Analytical spin decay, mirrored directions, landing spin, anchor values, interpolation bounds, boundary continuity, the high-Re drag blend and the low-Re lift bridge |
| `UOpenGolf.Physics.Wind` | Calm air unchanged; head-, tail- and crosswind direction and size; a ball moving with the air feels gravity only |
| `UOpenGolf.Physics.FlightScopeAnalysis` | Both screenshots; historical, decay, interpolation, lift/drag and timestep diagnostics; not an assertion of FlightScope accuracy |
| `UOpenGolf.Physics.ShotLibrary` | Replays all shots in `Data/ShotLibrary/` with every profile in `MakeShotLibraryProfiles()` and writes `Saved/ShotLibrary/results.csv`, including landing angle and spin; checks that every shot parses, completes and stays finite, not FlightScope accuracy |
| `UOpenGolf.Range.Shot` (game module) | A drive hit in the running range lands and stops within 1 yd carry / 2 yd total of the headless prediction (headless 248.44 / 262.53 yd since the 2026-10-16 flight corrections) |
| `UOpenGolf.Range.FrameRates` (game module) | Fixed-step outcomes across 30/60/144/5 FPS and 250 ms hitches, including the twelve-step catch-up cap |
| `UOpenGolf.Range.ReplaySelection` (game module) | Selected/repeated replay reaches the ball; corrected low wood lands at 121.5477 yd versus headless 121.5451 yd |
| `UOpenGolf.Range.RolloutSurfaces` (game module) | Same ball actor and geometry with mixed, all-fairway, and fringe-only tags; isolates rollout changes and checks neutral regime contact multipliers |

The golden values come from the ogolf-physics C# sources, not from this port.

### Regenerating fixtures

`Tools/ReferenceHarness/` is an optional .NET 9 console app. It compiles the Godot-free
ogolf-physics sources from a sibling checkout (`../ogolf-physics` next to this repository)
and runs a copy of `PhysicsAdapter`'s headless shot loop. It is not part of the Unreal build.

```powershell
dotnet run --project Plugins/OpenFairwayPhysics/Tools/ReferenceHarness -c Release
# or: ... -- <path-to-ogolf-physics> wood_low_test_shot.json@Green
```

Copy the printed values into `Private/Tests/PhysicsFixtureTest.cpp` and update the pinned
revision in the source headers when you port newer ogolf-physics changes.

### FlightScope shot library

The golden fixtures prove port parity only. To see how the physics compare with
FlightScope, `UOpenGolf.Physics.ShotLibrary` replays the recorded launch-monitor shots in
[`Data/ShotLibrary/`](../../Data/ShotLibrary/README.md) through `ShotSimulator::Simulate`
(fairway, 75 °F, sea level) with the current and historical profiles. Run the report from
the repository root after building:

```powershell
python Scripts/flightscope_compare.py
```

It prints engine minus FlightScope per shot for carry, total, roll and apex, then
summaries per profile and per club, and current-profile rollout by spin bin and landing
angle. It also shows the change since the last run with different results, so rerun it
after every physics change. Shots are read at run time, so adding a session needs no
rebuild. On Linux, run `python3 Scripts/flightscope_compare.py`.

To evaluate a physics change before making it the default, put it behind a profile field
that defaults to today's behaviour and add a named profile to `MakeShotLibraryProfiles()`
in `Private/Tests/ShotLibraryTest.cpp`. The report lists it beside `current` and
`historical`, and `--profile <name>` shows its per-shot table.

## Differences from ogolf-physics

- No exceptions: an invalid contact normal falls back to up, and launches are validated
  (`FLaunchData::IsValid`) before simulating.
- Single-precision math, as in C#. The port uses `MathF.PI`'s value, not the engine's
  double-precision pi, to stay bit-close.
- Out of bounds is 1000 m from the launch point (ogolf-physics uses the world origin), and
  a ball 100 m below its launch point stops instead of using Godot's terrain recovery.
- The ball actor spends the remainder of a landing step moving after impact, with
  another sweep to detect intervening geometry. This now matches the headless loop's
  contact timing closely enough to avoid selecting a different subsequent bounce.
- Current flight spin decay, neutral low-launch lift recovery, continuous calibration, the
  high-Re drag blend and the low-Re lift bridge intentionally differ from the pinned C#
  model; use `HistoricalReference()` when comparing golden fixtures.
- Wind, humidity and station pressure are additions; ogolf-physics has calm, dry air.

## Known gaps

- JSON ball profiles (`BallPhysicsProfile.FromJson`) are not ported; change `Profile` in C++.
- `SurfaceZone` area overrides, Terrain3D height queries and the lie resolver are not ported;
  use surface tags.
- Wind is uniform: no change with height and no gusts.
- No shot camera, tracer styling or HUD here (the game module supplies those).
- Physical-material surfaces are a possible later addition to tags.
- Packaged Linux builds have not been run. On Linux the full `UOpenGolf` automation run passes
  24 of 25 tests; `UOpenGolf.Range.ShotRecordingStorage` fails its "existing final file is
  never replaced" check (file I/O, not physics).
- Against the FlightScope shot library the current profile carries long (bias +2.4 yd) and
  rolls short (bias -2.9 yd); see the [baseline](../../Docs/FlightScopeAnalysis.md#shot-library-baseline-2026-10-16-linux).

## Plugin structure

```text
OpenFairwayPhysics.uplugin
Source/OpenFairwayPhysics/
  Public/OpenFairway/       Core (Core module only): profiles, GolfPhysics, surfaces,
                            ball profile and regimes, ShotSimulator
  Public/OpenFairwayTypes.h Blueprint types: surfaces, ball state, launch, shot result
  Public/OpenFairwayBall.h  Ball actor (Chaos sweeps and traces)
  Private/OpenFairway/      Core implementation
  Private/Tests/            Core and golden-fixture automation tests
Tools/ReferenceHarness/     Optional .NET harness that regenerates golden values
```

## References

- [ogolf-physics OpenFairway README](https://github.com/digitalhand/ogolf-physics/blob/e6d793ae9477efaf55dc9078d7260f76bb532a7d/addons/openfairway/README.md)
  and its C# physics sources at the pinned revision: implementation, regime-table
  and historical-fixture provenance. The local checkout at that revision was
  used to check the original regime documentation.
- A. R. Penner, [The physics of golf](https://doi.org/10.1088/0034-4885/66/2/202),
  *Reports on Progress in Physics* 66 (2003), 131–171: background on flight,
  impact and rolling models.
- P. W. Bearman and J. K. Harvey,
  [Golf Ball Aerodynamics](https://doi.org/10.1017/S0001925900007617),
  *Aeronautical Quarterly* 27 (1976), 112–122: wind-tunnel lift/drag measurements
  and ball-design dependence. This is background, not a claim that every fitted
  coefficient and empirical correction in the inherited implementation was
  independently verified against their data.
- A. M. Nathan, [The Effect of Spin-Down on the Flight of a Baseball](https://baseball.physics.illinois.edu/spindown.pdf)
  (2008), section II, equation 5 and footnote 5: the golf-ball measurements of
  Smits and Smith (1994) and SI-unit coefficient `2e-5` used by the current decay
  law. Their reported Reynolds range is roughly 100,000–250,000; extrapolation
  into slower flight still needs validation. This does not identify FlightScope's
  spin-decay implementation.
- NASA Glenn, [Viscosity](https://www.grc.nasa.gov/www/BGH/viscosity.html):
  Sutherland's temperature-dependent viscosity law used in the air model.
- [FlightScope Trajectory Optimizer](https://trajectory.flightscope.com/):
  external trajectory-model comparison, with inputs and outputs transcribed from
  the user's `Screenshot 2026-10-14 220535.png` and `Screenshot 2026-10-14 230552.png`.
  These are not measured outdoor landing data. The current 21 golden fixtures
  validate parity with the pinned C# implementation, not independent accuracy.

## License

MIT, as part of uopengolf. The equations and calibration come from ogolf-physics
(`addons/openfairway`, MIT), the maintainer's own project; see
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).
