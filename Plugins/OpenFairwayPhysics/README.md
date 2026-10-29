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
- [Wind and height](#wind-and-height)
  - [Headwinds](#headwinds)
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
| Air | Temperature, altitude, humidity, station pressure, and wind that grows with height; headwinds cost too little carry until recalibration (see [Headwinds](#headwinds)) |
| Headless simulator (`ShotSimulator`) | Ported from `PhysicsAdapter`; flat ground, one surface |
| Ball actor (`AOpenFairwayBall`) | Working on Windows and Linux; automated range shot within tolerance of the headless result |
| Surfaces | All seven presets; assigned by physical materials or tags; green speed set by stimp (`GreenStimpFeet`) |
| JSON ball profiles, surface zones, terrain probes | Not ported yet (see [Known gaps](#known-gaps)) |
| Linux | Editor/game build, the 33 headless `UOpenGolf` tests and a packaged game start pass (Unreal 5.8.3, 2026-10-18). After the later green-drag change, six physics tests pass; the range rollout tests have not rerun |

Ported from ogolf-physics revision `e6d793ae9477efaf55dc9078d7260f76bb532a7d`.

## Features

- Aerodynamic drag and Magnus lift from inherited polynomial fits, with calibrated
  spin-drag, high-launch and mid-spin corrections. The legacy low-launch lift boost
  is neutral in gameplay; the historical profile retains it.
- Drag eases into its high-Re cap from Re 175k instead of stepping up 7.8% at 200k, and
  lift bridges the Re 55k-68k band where two inherited fits fall to zero (see
  [Flight corrections](#flight-corrections)).
- Air density from temperature, altitude or a measured station pressure, and humidity.
  Wind acts on drag and lift through the ball's velocity relative to the air, at the ball's
  height (see [Wind and height](#wind-and-height)).
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
- Surfaces declared by physical material (`UOpenFairwayPhysicalMaterial`) or by tag.
- A presentation-only visual roll: the ball mesh turns as it rolls along the ground
  (`bVisualRoll`, on by default). It rolls without slipping by the frame's ground motion,
  capped at one radian per frame so fast rolls never read as turning backwards with
  motion blur off. Flight spin is not shown, since real spin rates alias at any frame rate.
  The roll changes only the actor's rotation; sweeps and traces ignore it. `PlaceAt` faces
  the ball down the aim line, so a marked ball's line points at the target on each re-tee.

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
2. Give the ground a material whose physical material is an **Open Fairway Physical
   Material** (the OpenGolfCourse plugin ships `PM_*` and `MI_Fairway`/`MI_Rough`), or tag
   ground components (or their actors) such as `OpenFairway.Surface.Green`. Undeclared
   ground uses the ball's **Default Surface**.
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
`WindSpeedMph` is the 10 m reading. `GetWindVelocityMps()` returns it, and
`GetWindVelocityAtHeightMps(HeightM)` returns the wind at a height above the ground.
`GetWindReferenceHeightM()` and `GetWindRoughnessM()` return the height profile, so scenery can show
the wind the ball flies through. Like the wind, they come from the launch while a shot moves.

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
can also be called directly with `FBallState` in SI units. `StepFlight` reads the wind at
`State.Position.Y`, the ball centre's height above the launch ground (`Radius` on the tee), so pass the
real position. An `FFlightEnvironment` from `FResolvedShotParams::Resolve` carries the 10 m profile,
and a zero position there means no wind. A default-constructed `FFlightEnvironment` has uniform wind
(`WindReferenceHeightM` 0).

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
| Surface under the ball | Lie resolver callback | The hit's `UOpenFairwayPhysicalMaterial`, or `OpenFairway.Surface.*` tags |

The ball is never a simulating Chaos rigid body, so Chaos cannot fight the calibrated
bounce and roll. Walls (normals flatter than about 45°) reflect the ball at 30% speed, as
in ogolf-physics. Obstacles, components or actors tagged `OpenFairway.Obstacle`
(`AOpenFairwayBall::GetObstacleTag()`) such as tree trunk colliders, always deflect: the
ball reflects off the horizontal part of the contact at 30% speed and never lands or
rests on them. A ball dropping onto a flat top leaves along its offset from the
obstacle's axis at 1 m/s or more. Ground probes look through obstacles. Canopy, components or
actors tagged `OpenFairway.Canopy` (`GetCanopyTag()`) with an overlap-only response on the trace
channel, such as a tree's foliage volume, never block: a ball in the air inside one keeps
`exp(-path / CanopyDecayLengthM)` of its speed and spin (4 m) and turns by a seeded angle of at most
`CanopyDeflectionDegPerSqrtM · √path` (6°), seeded by instance and step so a repeated shot repeats.
`PredictShot` and the headless reference ignore obstacles and canopy. Steps run at 120 Hz with up to 12 substeps per frame. After a long
stall, excess accumulated time is discarded rather than allowing unbounded catch-up.
Sustained rates below about 10 FPS therefore slow simulation relative to wall time.
The game module's `UOpenGolf.Range.FrameRates` regression checks matching carry,
total and simulation duration at 30/60/144/5 FPS and with 250 ms hitches. This verifies
fixed-step outcomes, not real-time playback at every render rate.

The game's ribbon tracer is separate from this actor's debug tracer. Its buffer/mesh
optimizations do not change the timestep, equations or collision queries. See the
[game validation notes](../../Docs/PhasesStatus.md#reference-measurements)
for CPU measurements and frame-time limitations.

## Surfaces

Declare a surface with an `UOpenFairwayPhysicalMaterial` (its **Surface** property) set on
the ground's material, material instance or Landscape layer, so a mesh drawn with that
material plays as that surface with no per-component setup. Contact sweeps and ground
probes return the hit's physical material, so complex collision (Landscape layers, per-face
materials) resolves per contact. Engine physical-material properties such as friction do
not affect the ball; OpenFairway's surface preset does.

`AOpenFairwayBall::ResolveSurface` takes the first match:

1. An `OpenFairway.Surface.<Name>` tag on the hit component (an explicit override).
2. An `UOpenFairwayPhysicalMaterial`: the hit's, else the component's simple-collision
   material (its physical-material override, then its graphics material's).
3. An `OpenFairway.Surface.<Name>` tag on the component's actor.
4. The ball's **Default Surface**.

| Surface | Tag | Kinetic friction | Rolling friction | Notes |
| --- | --- | --- | --- | --- |
| Fairway | `OpenFairway.Surface.Fairway` | 0.50 | 0.050 | Calibrated roll-out baseline |
| Soft fairway | `OpenFairway.Surface.FairwaySoft` | 0.56 | 0.070 | Wet or soft turf |
| Rough | `OpenFairway.Surface.Rough` | 0.62 | 0.095 | Grabs the ball |
| Firm | `OpenFairway.Surface.Firm` | 0.30 | 0.030 | Hard ground, long roll |
| Green | `OpenFairway.Surface.Green` | 0.58 | From stimp (ball's Green Stimp Feet, 10 ft) | Spin-back enabled; rolls at its stimp speed |
| Sand | `OpenFairway.Surface.Sand` | 0.65 | 0.30 | Soft: lower restitution and retention; landings plow (see below) |
| Fringe | `OpenFairway.Surface.Fringe` | 0.56 | 0.0725 | Close-cut collar |

`AOpenFairwayBall::GetSurfaceTag(Surface)` returns the tag. Values come from
`SurfaceCatalog` in `Private/OpenFairway/Surfaces.cpp`.

**Green speed.** The ball's **Green Stimp Feet** (8–15, default 10) sets every green's
rolling friction from the next launch; on the range, the `uopengolf.GreenStimp` console
variable writes it. That friction is solved together with the green's speed-dependent
grass drag, `FBallPhysicsProfile::GreenGrassViscosity`, so a stimpmeter ball still stops at
the stimp distance. The drag is **0.0009**: the original OpenFairway green value, below the
fairway's 0.0017. After a full-shot landing (about 2,000 rpm or more), a ball rolling from
fairway onto a green therefore goes farther than it would on fairway. `HistoricalReference()`
keeps the ported 0.011. See
[green and fairway by rolling speed](../../Docs/FlightScopeAnalysis.md#green-and-fairway-by-rolling-speed).

**Sand.** A landing in sand digs a crater. Each sand impact removes forward speed equal to
`FBallPhysicsProfile::SandPlowRatio` (default **2.5**) times the speed at which the ball drives
into the sand, so a shallow full-shot landing stays near its pitch mark. A grazing skull still
keeps some pace, and steep landings stop as before. The ported contact scaled its loss by the
square of the landing's sine, so a 15° landing kept about 95% of its pace and skipped out.
`HistoricalReference()` keeps that ported contact (ratio 0) for the golden fixtures.
`UOpenGolf.Physics.Sand` covers these cases.

## Ball profile and launch regimes

`AOpenFairwayBall::Profile` (`OpenFairway::FBallPhysicsProfile`) holds the calibrated
defaults: drag scale 1.01, green stimp 10 ft (set from Green Stimp Feet at launch), the flight/bounce/rollout constants, the
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
launches it; **H** repeats the selection. Inputs and available FlightScope targets
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
as the ball travels. In particular, the modeled green's grass viscosity was then
**0.011** (0.0009 since 2026-10-18; see [Surfaces](#surfaces)), versus **0.0017** on
fairway and **0.00245** on fringe. This coefficient
enters speed-dependent translational drag and spin damping. The green also uses
stimp-derived rolling resistance and excludes the fairway's spin multiplier from
that resistance. A 10 ft stimp check validates low-speed roll in this model;
it does not validate the whole high-speed post-impact response against FlightScope.

The corrected shot on a single flat fairway gives **37.69 yd roll / 159.24 yd total**,
whereas the mixed range reproduced **31.92 / 153.47 yd** with that green. Replacing only the green
with fairway, keeping the fringe, gives **36.71 / 158.26 yd**. Thus the original
range loses about 5.77 yd versus all fairway; this is a surface-response effect,
confirmed with identical actor and geometry. FlightScope's
Medium setting gives **38.7 / 158.5 yd**, with a shorter carry of 119.8 yd. Compare
equivalent surfaces before changing regime calibration to compensate for the
range layout. See the controlled surface experiment in the
[analysis](../../Docs/FlightScopeAnalysis.md#rollout-surface-isolation).

The test range assigns **Fairway to its 34 m fairway strip**, including the former
green/fringe area, through the `MI_Fairway` physical material; the side strips beyond
±17 m play as Rough. The low wood rolls 37.69 yd to 159.24 yd total down the line and
rests on Fairway. The ball reads each contacted mesh's physical material (or surface tag);
no surface is forced inside the physics solver or replay path. Other surface presets remain available for scenes that
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
previous results. Wind grows with height (see [Wind and height](#wind-and-height)) and has
no gusts. Humidity uses the Tetens
saturation pressure, with the dry air contributing its partial pressure.

## Wind and height

Wind is slowed by the ground: it is close to zero at the grass and grows with height. The
flight uses the neutral log wind profile over open ground:

`wind(h) = wind_ref * ln(h / z0) / ln(h_ref / z0)`

| Setting (`FAirConditions`) | Default | Meaning |
| --- | --- | --- |
| `WindReferenceHeightM` | 10 m | Height of the authored `WindSpeedMph`, as in weather reports. 0 keeps the wind uniform |
| `WindRoughnessM` | 0.03 m | Roughness length of open, short-grass ground |

| Height | Share of the authored wind |
| --- | --- |
| 1 m | 60% |
| 2 m (the flag) | 72% |
| 5 m | 88% |
| 10 m | 100% |
| 30 m | 119% |

- **Height:** the ball centre's height above the launch ground, not above the ground beneath
  the ball. The core has no world access. A ball below its launch ground gets no wind.
- **Limits:** the share is 0 at or below the roughness length and stops growing above 100 m
  (about 140%). Settings that cannot describe a profile (non-finite, or a reference height
  inside the roughness) fall back to uniform wind.
- **Effect:** high shots are pushed slightly more, and a low punch loses less to a tailwind
  and drifts less in a crosswind. The measured table is in `UOpenGolf.Physics.WindHeight`
  and `Docs/PhasesStatus.md`. Headwinds have a known calibration gap; see [Headwinds](#headwinds).
- **Roughness:** 0.03 m is class 3, "open flat terrain; grass, few isolated obstacles", of
  Davenport's terrain classification as adapted by Wieringa. It is also the roughness the
  [WMO observing guide](#references) corrects wind readings to, at the 10 m standard height.
- **Low-level step:** `FFlightEnvironment` defaults to uniform wind. `FResolvedShotParams::Resolve`
  copies the profile settings, so every simulated and in-world shot uses them. `GolfPhysics::StepFlight`
  reads the height from `State.Position.Y`.

### Headwinds

A headwind does not push the ball back directly. It raises the ball's speed through the air,
and both aerodynamic forces grow with that speed:
- **Drag** acts against the airflow and always shortens the shot.
- **Lift** comes from backspin (the Magnus effect) and acts across the airflow, so the ball
  climbs higher. Golfers call this ballooning.

Lift matters as much as drag:
- At launch, the 150 mph drive below gets lift of 1.22 times its weight.
- With lift switched off, the same drive carries 145 yd instead of 249 and peaks at 28 ft
  instead of 84.

A headwind therefore costs carry by adding drag, but it also adds lift. Low-launch shots have
less lift than their weight, and for them the extra lift can make up part of that loss, or all of it.

**Measured, 2026-10-29** (Linux, Unreal 5.8.4, DebugGame editor, gameplay profile, 75 °F, sea level):
- **The 30 mph rows** come from replays in Play with the wind from 0°.
- **The 10 and 20 mph rows** come from an offline replica of the flight loop. The replica is not
  in the repository. It reproduces the Play results within 1.5 yd of carry and 0.2 ft of apex.

| Shot | Calm | 10 mph | 20 mph | 30 mph |
| --- | --- | --- | --- | --- |
| Driver replay: 150 mph, 12.5°, 2,335 rpm | 248.4 yd, apex 83.9 ft | −12 yd, +8 ft | −29 yd, +17 ft | 196.3 yd (−52), apex 110.7 ft, lands at 66.5° |
| Low wood replay: 114.5 mph, 6.95°, 1,933 rpm | 121.6 yd, apex 15.0 ft | +5 yd | +9 yd | 130.9 yd (+9), apex 21.5 ft |
| Low punch: 95 mph, 8°, 4,200 rpm (replica only; calm matches `Physics.WindHeight`) | 106.3 yd, apex 19.4 ft | +10 yd | +14 yd | +13 yd |

Hang time shows where the low wood's gain comes from. The calm low wood stays up 2.74 s;
into 30 mph it stays up 3.56 s and lands at 23.9 m/s instead of 33.7 m/s:
- **Drag rises:** the ball arrives much slower.
- **Lift rises too:** at launch, lift is 76% of the ball's weight in calm air and 100% into the wind.

The wind's direction and frame are correct (`UOpenGolf.Physics.Wind`); the error is in the size of the response.

**Comparison with published aerodynamics.** The same replica was run with two independent
coefficient sets. Each was fitted to the calm carry of each shot by scaling its lift. Every
column below comes from the replica, so they compare like with like. In Play the low wood
gained 9 yd rather than 10.

| Change in carry into 30 mph | This model | Smits & Smith spin terms | Constant drag, linear lift |
| --- | --- | --- | --- |
| Driver | −52 yd | −87 yd | −67 yd |
| Low wood | +10 yd | +4 yd | −2 yd |
| Low punch | +13 yd | +4 yd | +1 yd |

- **Smits & Smith spin terms:** `Cd = 0.24 + 0.18 S`, `CL = 0.54 S^0.4`, from Smits and Smith
  (1994), without their Reynolds-number drag term.
- **Constant drag, linear lift:** `Cd = 0.25`, `CL ∝ S`. A simple form chosen for comparison;
  Bearman and Harvey (1976) give typical measured magnitudes.
- **Rules of thumb:** for a driver, about 12 yd lost into a 10 mph headwind
  ([USGTF](#references)), and a 300 yd carry falling to 259 yd into 20 mph
  ([Golf.com, from a Trackman study](#references)).

Low-launch shots are close to wind-neutral even with the published coefficients, so "always
shorter" is not a law of physics for them. Still, this model:
- gives them at least 2.5 times the larger published gain;
- loses about 20–40% less carry than the published sets for a driver in a strong headwind;
- loses less than the 20 mph rule of thumb (−12% against −14%).

At 10 mph its driver loss matches the 12 yd rule.

**Cause.** In this model, the drag coefficient at the driver and low-wood Reynolds numbers
(1.4–1.7 × 10⁵) is about 0.20. The Smits & Smith spin terms give about 0.25 at the same spin.
The inherited fits pair that low drag with low lift, so calm-air carries still match FlightScope.
In wind, the extra lift then outweighs the extra drag by more than it should.

**Status: not retuned.** The fix is a recalibration, and it changes every carry. It includes:
- drag raised to measured levels;
- lift and the regime overrides refitted against the [FlightScope shot library](#flightscope-shot-library);
- wind assertions in `UOpenGolf.Physics.Wind`: a driver loses about 25–35% of its carry into 30 mph,
  as the published sets do, and a low wood stays within ±5 yd of calm.

This joins the open recalibration (D6 in `Docs/PhasesStatus.md`).

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

- **Launch input:** mph, degrees and rpm. Positive HLA, spin axis and side spin are to
  the right.
- **Core:** SI units (m, m/s, rad/s) in the OpenFairway frame: +X downrange, +Y up,
  +Z right (right-handed, as in Godot).
- **Wind:** `WindFromDeg` is the direction the wind blows from, clockwise seen from above,
  measured from the +X target line: 0 is a headwind, 90 comes from the right and 180 is a
  tailwind. It is fixed to the world, so it does not turn with `AimYawDeg`. `WindSpeedMph` is
  the 10 m reading; the ball meets the wind at its own height.
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
| `UOpenGolf.Physics.ObstacleResponse` | Face-on, glancing, edge and flat-top obstacle contacts; tagging by component or actor |
| `UOpenGolf.Physics.CanopyResponse` | Canopy speed loss by path length (step-size independent), bounded seeded deflection, tagging; canopy is not an obstacle |
| `UOpenGolf.Physics.SurfaceResolution` | Resolution order: component tag, hit or component physical material, actor tag, default; plain physical materials are ignored |
| `UOpenGolf.Physics.GreenRollout` | After 2,000–6,000 rpm landings, a level stimp-10 green rolls a ball at 2–12 m/s at least as far as fairway; faster greens roll farther; the stimpmeter distance holds; the historical reference keeps the ported green |
| `UOpenGolf.Physics.GreenStimp` | Green Stimp Feet reaches predictions: the default matches the headless putt, a faster green rolls it farther, out-of-range stimps clamp, fairway unchanged |
| `UOpenGolf.Physics.VisualRoll` | Roll angle is distance over radius, the top moves with the motion, the per-frame cap, no motion and slopes |
| `UOpenGolf.Physics.FlightScopeAnalysis` | Both screenshots; historical, decay, interpolation, lift/drag and timestep diagnostics; not an assertion of FlightScope accuracy |
| `UOpenGolf.Physics.ShotLibrary` | Replays all shots in `Data/ShotLibrary/` with every profile in `MakeShotLibraryProfiles()` and writes `Saved/ShotLibrary/results.csv`, including landing angle and spin; checks that every shot parses, completes and stays finite, not FlightScope accuracy |
| `UOpenGolf.Range.Shot` (game module) | A drive hit in the running range lands and stops within 1 yd carry / 2 yd total of the headless prediction (headless 248.44 / 262.53 yd since the 2026-10-16 flight corrections) |
| `UOpenGolf.Range.FrameRates` (game module) | Fixed-step outcomes across 30/60/144/5 FPS and 250 ms hitches, including the twelve-step catch-up cap |
| `UOpenGolf.Range.ReplaySelection` (game module) | Selected/repeated replay reaches the ball; corrected low wood lands at 121.5477 yd versus headless 121.5451 yd |
| `UOpenGolf.Range.ObstacleContact` (game module) | A line drive into a tagged trunk rests short of it; a pitch onto a tagged flat-topped block slides off and rests on the ground; a line drive through an overlap-only canopy volume flies on without rebounding and carries at least 5 m less |
| `UOpenGolf.Range.RolloutSurfaces` (game module) | Default range (fairway strip by physical material, rough sides) and real lies on each, then mixed and fringe-only tags on the same geometry; checks that the green rolls farther than the same path tagged fairway and that regime contact multipliers are neutral |

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
- The green's grass drag is 0.0009, the original OpenFairway green value, instead of the
  ported 0.011, so a ball crossing a green from fairway rolls farther. The green's stimp
  friction is solved with it. `HistoricalReference()` keeps 0.011.

## Known gaps

- JSON ball profiles (`BallPhysicsProfile.FromJson`) are not ported; change `Profile` in C++.
- `SurfaceZone` area overrides, Terrain3D height queries and the lie resolver are not ported;
  use physical materials (per material or Landscape layer) or surface tags.
- Wind has no gusts, and its height is measured from the launch ground, not the terrain below the ball.
- Headwinds cost too little carry. A driver loses about 20–40% less than published aerodynamics
  into 30 mph, and low-launch shots gain 5–14 yd. See [Headwinds](#headwinds).
- No shot camera, tracer styling or HUD here (the game module supplies those).
- Physical-material surfaces are a possible later addition to tags.
- Packaged builds are only start-checked headless; ball flight in a rendered packaged game
  has not been reviewed on either platform.
- Against the FlightScope shot library the current profile carries long (bias +2.4 yd) and
  rolls short (bias -2.9 yd); see the [baseline](../../Docs/FlightScopeAnalysis.md#shot-library-baseline-2026-10-16-linux).
- No measured green-rollout reference exists. `UOpenGolf.Physics.GreenRollout` checks only
  that a green out-rolls fairway after full-shot landings. With the 0.0009 green drag, a putt
  at a given speed rolls farther than under the ported model; see
  [green and fairway by rolling speed](../../Docs/FlightScopeAnalysis.md#green-and-fairway-by-rolling-speed).

## Plugin structure

```text
OpenFairwayPhysics.uplugin
Source/OpenFairwayPhysics/
  Public/OpenFairway/       Core (Core module only): profiles, GolfPhysics, surfaces,
                            ball profile and regimes, ShotSimulator
  Public/OpenFairwayTypes.h Blueprint types: surfaces, ball state, launch, shot result
  Public/OpenFairwayBall.h  Ball actor (Chaos sweeps and traces)
  Public/OpenFairwayPhysicalMaterial.h  Physical material that declares a surface
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
- A. J. Smits and D. R. Smith, "A new aerodynamic model of a golf ball in flight," in
  *Science and Golf II: Proceedings of the 1994 World Scientific Congress on Golf*,
  A. J. Cochran and M. R. Farrally (eds.), E & FN Spon, London (1994), 340–347. Cited
  through Nathan's paper above. Source of the spin-decay law and of the spin-dependent drag and
  lift terms used in the [Headwinds](#headwinds) comparison.
- World Meteorological Organization,
  [Guide to Meteorological Instruments and Methods of Observation (WMO-No. 8)](https://www.weather.gov/media/epz/mesonet/CWOP-WMO8.pdf),
  seventh edition (2008; NOAA-hosted copy), Part I, Chapter 5 "Measurement of surface wind":
  - the 10 m standard height over open terrain;
  - the annex table of Davenport's terrain classes as adapted by Wieringa, whose "open" class
    has z0 = 0.03 m;
  - the correction of readings to z = 10 m and z0 = 0.03 m.
- A. Rice, [How weather conditions affect your golf ball](https://golf.com/instruction/weather-conditions-effect-golf-ball/),
  Golf.com (24 June 2019): from a year-long study with Trackman, a 300 yd drive carries 259 yd
  into a 20 mph headwind.
- M. Harman, [Editorial – Wind, Temperature and The Open](https://www.usgtf.com/?p=29996),
  USGTF (1 August 2021): a driver loses about 12 yd of carry into a 10 mph headwind and gains
  about 9 yd downwind.
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
