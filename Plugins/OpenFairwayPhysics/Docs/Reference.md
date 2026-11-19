# OpenFairway Physics

Paths in this reference are relative to the plugin root unless stated otherwise.

Native C++ golf ball physics for Unreal Engine 5.8: flight, impact, bounce, roll and
rest from [ogolf-physics](https://github.com/digitalhand/ogolf-physics), plus an Unreal
ball actor that uses Chaos scene queries for contact. No .NET runtime and no Godot.

Part of [uopengolf](../../../README.md). Original code is MIT; ogolf-physics is the
maintainer's own MIT project, so the port is covered by this repository's license.

## Historical verification

These dates describe earlier builds. See the [current status](../../../Docs/PhasesStatus.md) for later evidence.

| Area | State |
| --- | --- |
| Core equations (flight, impact, ground, rest) | Ported, then corrected for spin decay, calibration continuity, excess low-launch lift, the high-Re drag step and the low-Re lift dead band; historical profile matches 21 C# fixtures |
| Air | Temperature, altitude, humidity, station pressure, and wind that grows with height; headwinds cost too little carry until recalibration (see [Headwinds](#headwinds)) |
| Headless simulator (`ShotSimulator`) | Ported from `PhysicsAdapter`; one surface on level or planar sloped ground |
| Ball actor (`AOpenFairwayBall`) | Working on Windows and Linux; automated range shot within tolerance of the headless result |
| Surfaces | All seven presets; assigned by physical materials or tags; green speed set by stimp (`GreenStimpFeet`); turf landing loss read from the impact and fitted to launch-monitor rollout (2026-11-17, see [Turf rollout calibration](#turf-rollout-calibration)); see [Greens](#greens) |
| JSON ball profiles, surface zones, Terrain3D height queries | Not ported yet (see [Known gaps](#known-gaps)) |
| Linux | Editor/game build, the 33 headless `UOpenGolf` tests and a packaged game start pass (Unreal 5.8.3, 2026-10-18). After the later green-drag change, six physics tests pass; the range rollout tests have not rerun |
| Windows | Editor build and all 78 `UOpenGolf` tests pass (2026-11-09), including `UOpenGolf.Physics.Sand` after the sand rolling-friction change. After the 2026-11-17 turf changes, `UOpenGolf.Physics` passes 21 of 21 and `UOpenGolf.Range` 36 of 36 (`Saved/Automation/20261117-145826-71a0b18f`, `20261117-145843-45790213`) |

Ported from ogolf-physics revision `e6d793ae9477efaf55dc9078d7260f76bb532a7d`.

## Features

- Drag and Magnus lift as curves in the spin ratio, plus a measured low-Reynolds drag rise,
  fitted to reference flights from 36 to 175 mph and integrated with RK4; no club, launch-angle or
  regime terms (see [Flight model](#flight-model)).
- `LegacyCalibrated()` keeps the inherited polynomial fits with their calibrated spin-drag,
  high-launch and mid-spin corrections, the high-Re drag blend and the low-Re lift bridge (see
  [Flight corrections](#flight-corrections)).
- Air density from temperature, altitude or a measured station pressure, and humidity.
  Wind acts on drag and lift through the ball's velocity relative to the air, at the ball's
  height (see [Wind and height](#wind-and-height)).
- Impact impulses with speed- and spin-dependent restitution and friction-limited tangential
  response. On turf the landing loss is read from the impact itself (its angle and backspin); on
  green and sand a first-impact "bite" is chosen per landing bucket (run, release, check, spin-back).
  Impacts read the backspin across the ball's travel, so sidespin never checks a ball.
- Ground roll with sliding friction until rolling, rolling resistance, linear and speed-squared
  grass drag, air drag, slope drive and slope resistance, and rest detection. A topped full shot,
  struck along the ground, rolls with its own share of the grass drag (see [Topped shots](#topped-shots)).
- Seven surface presets: fairway, soft fairway, rough, firm, green (set by stimp; see [Greens](#greens)), sand, fringe.
  The turf presets are fitted to launch-monitor rollout.
- Launch-regime drag and lift overrides inherited from ogolf-physics.
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

- Unreal Engine 5.8 with C++ (the same toolchain as the [project](../../../Docs/Development.md#prerequisites)).
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
// Result.LandingSpinRpm, Result.bFirstImpactSpinback, Result.bAtRest

OpenFairway::FAirConditions Air;  // 75 F, sea level, dry and calm by default
Air.RelativeHumidity = 0.5f;
Air.WindSpeedMph = 10.0f;
Air.WindFromDeg = 0.0f;  // headwind
const auto IntoWind = OpenFairway::ShotSimulator::Simulate(
    Launch, OpenFairway::ESurface::Fairway, OpenFairway::FBallPhysicsProfile(), Air);
```

Check `bAtRest` before treating `TotalM` as final; false means the 120 s limit
(`ShotSimulator::DefaultMaxTimeSeconds`) ran out. The ball actor's `MaxShotSeconds` has the same
default and ends a longer shot as a `SafetyStop`.
The core functions (`GolfPhysics::StepFlight`, `ResolveImpact`, `StepGround`, `CanRest`)
can also be called directly with `FBallState` in SI units. `StepFlight` reads the wind at
`State.Position.Y`, the ball centre's height above the launch ground (`Radius` on the tee), so pass the
real position. An `FFlightEnvironment` from `FResolvedShotParams::Resolve` carries the 10 m profile,
and a zero position there means no wind. A default-constructed `FFlightEnvironment` has uniform wind
(`WindReferenceHeightM` 0). `StepGround` ignores `State.Position` for wind: a ball on the ground is one
radius above its lie, so it meets the wind at that height wherever the lie is (none under the 10 m
profile, all of it when the wind is uniform). Its air drag reads only the ball's velocity and spin and
`FFlightEnvironment::BallDragScale`, not the launch's regime multiplier or high-launch drag.

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
[game timing workflow](../../../Docs/RangeAssets.md)
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

| Surface | Tag | Kinetic friction | Rolling friction (ported) | Grass drag (ported) | Form drag, 1/m | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Fairway | `OpenFairway.Surface.Fairway` | 0.50 | 0.0469 (0.050) | 0.0242 (0.0017) | 0.0086 | Fitted to launch-monitor rollout; about 7.4 ft on a stimpmeter |
| Soft fairway | `OpenFairway.Surface.FairwaySoft` | 0.56 | 0.0657 (0.070) | 0.0342 (0.0024) | 0.0121 | Wet or soft turf |
| Rough | `OpenFairway.Surface.Rough` | 0.62 | 0.0891 (0.095) | 0.0456 (0.0032) | 0.0162 | Grabs the ball; about 3.9 ft |
| Firm | `OpenFairway.Surface.Firm` | 0.30 | 0.0281 (0.030) | 0.0142 (0.0010) | 0.0051 | Hard ground, long roll |
| Green | `OpenFairway.Surface.Green` | 0.58 | From stimp (ball's Green Stimp Feet, 10 ft) | 0.0009 | 0 | Spin-back enabled; rolls at its stimp speed; see [Greens](#greens) |
| Sand | `OpenFairway.Surface.Sand` | 0.65 | 0.45 (profile; ported 0.30) | 0.012 | 0 | Soft: lower restitution and retention; landings plow (see below) |
| Fringe | `OpenFairway.Surface.Fringe` | 0.56 | 0.0680 (0.0725) | 0.0349 (0.00245) | 0.0124 | Close-cut collar |

The five turf surfaces also share the landing loss read from each impact (`LandingAngleLoss` 0.665,
`LandingSpinLoss` 0.464) and the grass drag shares for a full shot struck along the ground
(`StruckViscosityScale` 0.26, `StruckFormDragScale` 0.69; see [Topped shots](#topped-shots)); green
and sand keep the ported landing and have no struck drag.

`AOpenFairwayBall::GetSurfaceTag(Surface)` returns the tag. Values come from
`SurfaceCatalog` in `Private/OpenFairway/Surfaces.cpp`. The five turf surfaces play the
recalibrated catalog (`FBallPhysicsProfile::bRecalibratedTurf`, on by default);
`SurfaceCatalog::Get(Surface)` without it returns the ported values, which `HistoricalReference()`
keeps. See [Turf rollout calibration](#turf-rollout-calibration).

Green speed, grass drag, spin-back and slopes are covered in [Greens](#greens).

**Sand.** A landing in sand digs a crater. Each sand impact removes forward speed equal to
`FBallPhysicsProfile::SandPlowRatio` (default **2.5**) times the speed at which the ball drives
into the sand, so a shallow full-shot landing stays near its pitch mark. A grazing skull still
keeps some pace, and steep landings stop as before. The ported contact scaled its loss by the
square of the landing's sine, so a 15° landing kept about 95% of its pace and skipped out.
`HistoricalReference()` keeps that ported contact (ratio 0) for the golden fixtures.
A rolling ball meets `FBallPhysicsProfile::SandRollingFriction` (default **0.45**) in place of
the catalog's ported 0.30, so a ball that rolls into a bunker at 4 m/s stops after about 1.7 m
instead of 2.5 m; `HistoricalReference()` keeps 0.30. `UOpenGolf.Physics.Sand` covers these cases.

### Turf rollout calibration

Recorded range shots (2026-11-13) showed three rollout faults on fairway. Flight was not the
cause: across a 124-shot launch-monitor export (lm), the flight of the time (now
`LegacyCalibrated()`) had carry, apex and landing angle biased by less than 1.1 yd, 1.9 ft and 0.3°
against lm's own results, though carry was off by 4.1 yd per shot. The turf below was fitted with
that flight; the gameplay flight has since changed (see [Flight model](#flight-model)). Shots here
are described by their launch values. The club labels on the export and the recordings are the club selected, not a measurement
(the export has club speed and smash factor as 0), and they are often wrong.

- **Ground-launched shots ran away.** A 67.5 mph shot launched at −0.14° rolled 129 yd on the
  range and would have run 220 yd on flat fairway; a 100.2 mph shot at 0.78° ran 263 yd. The
  first-impact loss fades in over 0.75–2 m/s of normal impact speed, so a ball that starts along
  the ground lost nothing, and the fairway's own resistance could not stop it: the ported fairway
  measures about 13 ft on a stimpmeter, faster than the 10 ft green.
- **A tilted spin axis checked the ball.** Impacts read the total spin, so a 107.8 mph shot with an
  85° axis (−401 rpm back, 4581 rpm side) was checked as if it had 4600 rpm of backspin and rolled
  18 yd. On flat ground, sidespin is spin about the contact normal and cannot touch the turf.
- **Full shots stopped dead.** On top of the friction impulse, the first impact multiplied the
  forward speed by `0.55 × clamp(1 − rpm/8000, 0.4, 1)` and then applied the landing bite: a
  109 mph, 13.8°, 4200 rpm shot landing at 19 m/s along the ground left at 3 m/s and rolled 4.5 yd;
  lm's mid-launch shots above 4000 rpm roll 10.3 yd on average.

**The turf contact now.** An impact on turf is the standard rigid-body contact plus one fitted loss
read from the impact itself:

1. restitution from the ported curve (with its spin cut), giving the normal impulse;
2. the Coulomb-capped `2/7 · slip` friction impulse;
3. the turf keeps `exp(−k · vn/vt) · max(0, 1 − min(c · R·ωb, G) / vt)` of the tangential velocity
   and the contact-plane spin, where `vn` and `vt` are the incoming normal and tangential speeds, `R`
   the ball radius, `ωb` the backspin across the travel (`LandingAngleLoss` k and `LandingSpinLoss` c
   on `FSurfacePhysicsSettings`) and `G` the contact's Coulomb bound per unit mass, kinetic friction
   times the normal impulse (the same bound as step 2).

A steeper landing digs in and keeps less; backspin grips; a ball skimming the turf (`vn` near 0)
keeps nearly all its pace. Like friction, the grip needs load: without the bound `G`, a touch at an
arbitrarily small normal speed took the whole backspin loss (68.7% kept at 2 m/s with 2000 rpm, none
at 8000 rpm), which could check a slow, spinning ball at a terrain seam. With it, the loss fades out
as `vn` goes to 0 and full-shot landings, far from the bound, are unchanged. On turf this one law
replaces three empirical layers on the first impact (the retention multiplier with its spin clamp,
its normal-speed ramp, and the bite chosen by landing bucket) and the 0.70–0.85 retention on later
impacts. Nothing about the launch reaches the landing: two shots that arrive with the same velocity
and spin leave the same way. The one place the launch reaches the roll is a full shot struck into or
along the ground (see [Topped shots](#topped-shots)). Greens and sand keep the ported landing.

| | Ported | 2026-11-16 recalibration | Current (2026-11-17) |
| --- | --- | --- | --- |
| Spin read by impacts | Total | Backspin across the travel (`bAxisAwareSpin`) | Same |
| Spin read by ground friction and the rest check | Total | Contact-plane spin; a ball that only pivots can rest | Same |
| Turf landing loss | `0.55 × clamp(1 − rpm/8000, 0.4, 1)`, ramped in over 0.75–2 m/s of normal speed, then the landing bucket's bite | Same, with 0.95, a 0.75–5 m/s ramp and 35% of the bite | `exp(−0.665 vn/vt) · max(0, 1 − min(0.464 R·ωb, G)/vt)` on every turf impact |
| Impact-spin friction multiplier on turf (`RollingSpinScale`, `SlidingSpinScale`) | 1, 1 | 0, 0 | 0, 0 |
| Fairway rolling friction | 0.050 | 0.050 | 0.0469 |
| Fairway grass drag, linear (speed squared, 1/m) | 0.0017 (0) | 0.014 (0.010) | 0.0242 (0.0086) |
| Fairway on a stimpmeter | 13.1 ft | 9.2 ft | 7.4 ft |
| Ground step (`FRolloutProfile::bCoupledContact`) | Split: drag and rolling resistance, friction only once slip passes 0.05 m/s | Coupled: drag (integrated exactly), rolling resistance, then a friction impulse that ends each step rolling | Coupled, with air drag on the rolling ball (its own state only: wind one radius above the lie, no launch-regime drag) |
| A launch along or into the ground | One flight step, then a landing | Same | Contacts the ground at t = 0, with no carry; a full shot rolls with the struck grass drag ([Topped shots](#topped-shots)) |

The coupled step removes a numerical artifact: the split step left a rolling ball slipping after
every drag update and corrected it only past a slip threshold. `UOpenGolf.Physics.RolloutRegression`
checks that the ground step never adds energy, leaves a rolling ball with no slip, and gives totals
within 1% (or 0.25 yd) at 240 and 480 Hz. The other turf surfaces keep their ported ratios of grass
drag and rolling friction to fairway, and take the same landing loss and struck drag shares, since
only fairway has reference shots.

**The fit.** `UOpenGolf.Calibration.Rollout` (an opt-in test, outside `UOpenGolf.Physics`) fits the
124-shot launch-monitor export (lm) in the [local reference data](../../../Assets/Data/README.md#where-the-data-lives) on flat
fairway at 75 °F and sea level, in two stages, and writes every candidate to
`Saved/ShotLibrary/calibration.csv`. It groups shots by launch values only.

1. **Turf values.** k, c, rolling friction and both grass drags, scored on roll error of the 115
   airborne shots, plus a twentieth of the error of the three near-ground shots that still fly
   (0.6–1.6°). The six struck along the ground are left to stage 2. Constraints:
   - every shot comes to rest;
   - total changes by at most 14% per 1° of launch and 2.8% per 0.1°, except across 0° (the
     regression test allows 15% and 3%);
   - the fairway is slower than the green and at least 6 ft on a stimpmeter (a judgment; the fit
     does not reach it);
   - rough holds a resting ball on a slope at least 1° steeper than a green does, as
     `UOpenGolf.Physics.SlopeRollout` requires.

   The result replaces the first 2026-11-17 fit (k 0.84, c 0.14, rolling friction 0.0735, grass
   drag 0.0229 and 0.0039, 6.0 ft). That fit sat on both the stimp floor and the 14% limit at the 0°
   crossing. An unconstrained run reached the same error with fairway rolling friction 0.029, which
   would let rough hold less than a green.
2. **Struck drag.** The two struck-along-the-ground scales, on the six shots launched below 0°,
   over the stage 1 turf.

The run of 2026-11-17 is `Saved/Automation/20261117-145705-5a36a301`.

Roll, engine minus lm, in yards (bias / mean absolute error):

| Launch class | Shots | lm roll | Ported | 2026-11-16 | First 2026-11-17 fit | Current |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Ground, launch < 2.5° | 9 | 86.6 | +211.0 / 211.0 | −1.6 / 22.2 | −25.3 / 29.4 | −0.1 / 0.8 |
| Low, 2.5–9° | 11 | 35.5 | −2.1 / 6.8 | +7.6 / 7.6 | +3.6 / 3.6 | +2.3 / 2.3 |
| Mid 9–20°, < 4000 rpm | 33 | 22.7 | −3.6 / 6.7 | −0.5 / 2.3 | −1.4 / 2.0 | −1.1 / 1.8 |
| Mid 9–20°, ≥ 4000 rpm | 48 | 10.3 | −1.9 / 2.6 | +0.2 / 1.1 | +0.2 / 1.3 | +0.4 / 1.0 |
| High, ≥ 20° | 23 | 3.6 | −1.8 / 1.9 | −0.2 / 0.6 | +1.0 / 1.0 | +0.4 / 0.6 |
| Airborne (all but ground) | 115 | | −2.4 / 4.0 | +0.6 / 2.0 | +0.2 / 1.7 | +0.1 / 1.3 |
| All | 124 | 20.1 | +13.1 / 19.1 | +0.4 / 3.4 | −1.6 / 3.7 | +0.1 / 1.2 |
| Total distance, all | 124 | | +9.6 / 21.6 | −0.6 / 6.0 | −2.7 / 5.4 | −0.9 / 4.0 |

"Ported" is `HistoricalReference()`, which also has the older flight model. Total error is now
mostly flight: carry averages 4.1 yd of error (see [Known gaps](#known-gaps)). The three near-ground
shots that fly (0.6–1.6°) roll within 0.8 yd of lm but carry 6–16 yd short of it, so they total
7–16 yd short. Other limits: the export has no shot faster than 121 mph ball speed and none on rough
or the other turf surfaces. With the legacy flight the standard drive totalled 268.3 yd (264.3 yd
under the first 2026-11-17 fit, 273.8 yd under the 2026-11-16 values, 262.5 yd before them); with
the 2026-11-18 flight it carries 244.0 yd and totals 261.6 yd.

`UOpenGolf.Physics.RolloutRegression` pins:

- the tilted-axis fault and the mid-launch release;
- the launch-angle continuity on each side of 0°;
- the landing loss's dependence on the impact alone, and its grip bound;
- the ground step's independence from the launch.

It also requires ground-launched shots to come to rest. It sets no distance target for the recorded
ones, since no reference result exists for those.

### Topped shots

The launch-monitor export (lm) reports six of its shots with a launch below 0°, no carry and no apex. These are
topped shots, struck into the ground at full ball speed. lm totals them at about 1.2 yd per mph:
59.6 mph at −0.8° goes 76.0 yd, and 103.7 mph at −0.2° goes 124.6 yd. A model that reads only the
ball's own state cannot also match lm's shots that fly first. lm rolls nearly the same ground
state much less after a short flight:

| lm shot | Starts along the ground at | lm roll |
| --- | --- | ---: |
| 113.9 mph, +0.6° (flies 37.9 yd) | about 44 m/s, landing at 0.8° | 69.2 yd |
| 103.7 mph, −0.2° | 46.4 m/s, at 0.2° | 124.6 yd |
| 91.4 mph, +1.4° (flies 24 yd) | about 36 m/s, landing at 1.5° | 57.5 yd |
| 70.8 mph, −1.8° | 31.7 m/s, at 1.8° | 90.1 yd |

The best state-only fits left the topped shots 22–30 yd short and made 2.5–9° launches roll 9–11 yd
too far. So a full shot struck into or along the ground keeps its own grass drag for the rest of its
roll, on every surface it rolls over: the turf's linear grass drag times `StruckViscosityScale` (0.26)
and its speed-squared drag times `StruckFormDragScale` (0.69). Rolling friction, the stimpmeter speed,
landings and every other shot are unchanged. This is the one rule in the roll that reads how the shot
started (`GolfPhysics::StruckAlongGround`).

- **Which shots:** those whose launch velocity points into or along the ground, so they contact it
  at t = 0. On flat ground that is a launch at or below 0°.
- **Putts are exempt.** The share ramps in with launch speed over 12–18 m/s (27–40 mph). The same
  ramp tells a putt from a topped shot when no spin is measured. A putt or a Texas wedge at 0° rolls
  as before, and its total stays continuous across 0°.
- **Result:** the six topped shots finish within 0.9 yd of lm on average (worst 2.5 yd). Before
  this rule they stopped 32–47 yd short.
- **Cost:** at full speed, total distance steps at 0°. At 60 mph and 2000 rpm it is 78.2 yd at 0°
  against 41.5 yd at 0.1°; at 100 mph, 121.9 against 68.3 yd. lm's own results step the same way.
  On each side of 0°, total still changes by at most 15% per 1° and 3% per 0.1°.
- Green and sand have no struck drag (scales 1), nor does `HistoricalReference()`.

## Greens

Everything that makes a green play like a green: the surface preset, its speed, how
shots land and roll on it, slopes, how a course declares one, and how it is tested.
The equations are the ported ogolf-physics ones; the only deliberate change is the
green's grass drag (see [Grass drag and the ported value](#grass-drag-and-the-ported-value)).

### Green surface preset

`SurfaceCatalog` (`Private/OpenFairway/Surfaces.cpp`) defines the green with the same
values as ogolf-physics `SurfacePhysicsCatalog.Green`:

| Setting | Value | Effect |
| --- | --- | --- |
| Kinetic friction | 0.58 | Sliding grip at impact and until the ball rolls |
| Rolling friction | From the stimp (0.0747 at 10 ft) | Constant roll resistance; see [Green speed](#green-speed-stimp) |
| Grass viscosity | 0.0009 (ported 0.011) | Speed-dependent drag and spin damping |
| Critical angle θc | 0.36 rad | Penner contact angle; steeper landings grip |
| Spin-back response scale | 1.12 | Reverse-spin impulse at impact (fairway 0.78) |
| Spin-back theta boost | up to 0.12 rad | Extra critical angle for high-spin landings |
| Spin-back spin window | 3,500–5,500 rpm | Spin over which the boost ramps in |
| Spin-back speed window | 8–20 m/s | Impact speed over which the boost ramps in |
| Rolling spin scale | 0 | Impact spin does not scale roll resistance: every ball rolls at the stimp speed |
| Restitution, retention, bounce | Neutral (1, 1, default) | The ported rebound and retention rules and the landing bucket's bite; turf reads its landing loss from the impact instead |

### Green speed (stimp)

The ball's **Green Stimp Feet** (8–15, default 10) sets every green's rolling friction
from the next launch; out-of-range values clamp and a non-finite value plays as 10. On
the range, the `uopengolf.GreenStimp` console variable writes it; headless callers set
`FBallPhysicsProfile::GreenStimpFeet`. Stimp never changes another surface.

`GreenSpeed::RollingFriction` solves for the constant deceleration that, together with
the green's speed-dependent grass drag (`FBallPhysicsProfile::GreenGrassViscosity`) and, on
the current profile, air drag on the rolling ball (`bAirDrag`, with the coupled ground step;
`HistoricalReference()` leaves it out, as ogolf-physics does), stops a ball leaving a
stimpmeter at **1.83 m/s** at the stimp distance. The roll
distance has no closed-form inverse, so it bisects (32 iterations, double precision);
rolling friction is `7·a0 / (5·g)`.

| Stimp, ft | Rolling friction (current: drag 0.0009, air drag) | Hold angle | Rolling friction (historical: ported drag 0.011, no air drag) | Hold angle |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 0.0943 | 8.6° | 0.0711 | 6.5° |
| 10 | 0.0747 | 6.8° | 0.0518 | 4.7° |
| 11 | 0.0675 | 6.2° | 0.0448 | 4.1° |
| 12 | 0.0616 | 5.6° | 0.0390 | 3.6° |
| 15 | 0.0485 | 4.4° | 0.0264 | 2.4° |

The hold angle is the steepest slope a resting ball stays on (see
[Slopes and break](#slopes-and-break)).

### Grass drag and the ported value

The green's grass drag is **0.0009**, the original OpenFairway green value, below the
fairway's 0.0017. ogolf-physics uses **0.011**, which it fitted to measured putt speeds
from 10 to 60 ft. Both stop a stimpmeter ball at the stimp distance, because the
rolling friction is solved with the drag, but they differ at other speeds:

- With 0.0009, a ball rolling from fairway onto a green after a full-shot landing
  (about 2,000 rpm or more) goes farther than it would on fairway.
- With 0.011, a fast roll stops shorter on the green than on fairway, and putts match
  the reference's putt bands (a 6 mph putt rolls 20.2 ft at stimp 11; it rolls 20.7 ft at
  stimp 10 with 0.0009 on the current profile).

`FBallPhysicsProfile::HistoricalReference()` keeps the ported 0.011, and every green
golden value uses it. The rolling-speed comparison above describes the tradeoff;
see [Does the regime limit rollout?](#does-the-regime-limit-rollout) for a controlled surface comparison.

### Landing on a green (spin-back and check)

A green's first impact uses the launch's [landing bucket](#landing-buckets): Run keeps
the neutral bounce, Release and Check add spin "bite" that shortens the release, and
Spinback (`P4` spin with `V2`–`V4` launch) may also reverse the ball by up to 1.2 m/s.
The green's stronger spin-back response (scale 1.12 with its theta boost, against
fairway's 0.78 and none) is what reverses the ball: the same flop on fairway stops dead. `FShotSimulationResult` reports
`FirstImpactTangentInMps`, `FirstImpactTangentOutMps` (negative after spin-back) and
`bFirstImpactSpinback`, as ogolf-physics' `first_impact_*` diagnostics.

On the historical profile, the reference's own bands hold: a wedge (Check) rolls out
1.1 yd versus 5.8 yd with no bite, a mid-iron (Release) 7.0 yd versus 9.4 yd, and the
checked and flop wedges draw back 1.6 yd on green and not at all on fairway.

### Slopes and break

`GolfPhysics::StepGround` drives the ball down the slope with gravity and resists it
with rolling friction and grass drag. A ball slower than 0.05 m/s and 8 rad/s stays put
while the slope is within its hold angle, `atan(rolling friction × 1.6)`
(`FRolloutProfile::SlopeResistanceRatio`); beyond that it rolls. A stimp-10 green holds
a ball on up to 6.8°, fairway 4.3° and rough 8.1°, so a slope rough holds can still roll
a ball on a green.

The ball actor uses each contact's normal (`FloorNormal`), so a curved green breaks
putts. `ShotSimulator::Simulate` takes a `GroundNormal` for a planar slope through the
launch point:

```cpp
const float Slope = FMath::DegreesToRadians(2.0f);  // downhill along the target line
const auto Downhill = OpenFairway::ShotSimulator::Simulate(Launch, OpenFairway::ESurface::Green,
    OpenFairway::FBallPhysicsProfile(), OpenFairway::FAirConditions(),
    OpenFairway::ShotSimulator::DefaultMaxTimeSeconds, FVector3f(FMath::Sin(Slope), FMath::Cos(Slope), 0.0f));
```

Neither this port nor ogolf-physics models grain (nap) or green firmness separately
from the surface preset.

### Building greens in a course

A green is any ground whose material carries `PM_Green` (an
`UOpenFairwayPhysicalMaterial` with **Surface** Green), or a component tagged
`OpenFairway.Surface.Green`; see [Surfaces](#surfaces) for the resolution order.
In `OpenGolfCourse`:

- `MI_Green` and `MI_CoastalGreen` reference `PM_Green`, so a mesh drawn with either
  plays as green.
- `DA_CoastalGreen` is the green surface preset for the terrain builder, which sets
  `PM_<Surface>` on each region mesh (`OpenGolfTerrainBuilder.cpp`).
- The builder's Landscape gets only the area's default physical material. A green
  painted as a Landscape layer needs that layer's physical material set to `PM_Green`.
- `MI_Tee` and `MI_CoastalTee` carry no physical material, so ground drawn with them
  plays as the ball's **Default Surface** (Fairway). No shipped map uses them.

### Cup and hole-out

The green is a solid surface, so the cup is a capture rule, as in ogolf `game/hole/CupCapture.cs`; a 108 mm hole in a
0.5 m ground mesh would meet the ball as a wall. `OpenFairway/CupCapture.h` (Core module only) holds it:

| Rule | Value |
| --- | --- |
| Cup | Regulation: 4.25 in (108 mm) across, 4 in (101.6 mm) deep |
| Rolling capture | Lip-entry model (Holmes 1991): a ball crossing the cup along a chord drops if it falls one ball radius before reaching the far rim, `MaxCaptureSpeed(offset) = 2 sqrt(r_cup^2 - offset^2) sqrt(g / 2 r_ball)`: about 1.64 m/s through the centre, 0 at the rim. A ball that stops over the cup is holed; a faster one crosses the open cup (Lip reaction), and a wider one rolls past |
| Lip reaction | `AdvanceCrossing` integrates ballistic motion in a local cup frame, resolves sphere contact with the rim/outer green at impact time, and returns consumed time. The actor rotates gravity, velocity and spin into that frame. Restitution 0.1 and friction 0.3 are inherited, uncalibrated defaults; hops are physical model outputs, not height-capped animations. No unconditional upward snap is applied at the far edge. |
| Dunk | A descending contact inside the cup at green level, from flight or a bounce |
| Height gate | The centre within two ball radii (a contact within one) of the cup, so a ball on the flagstick never counts |
| Drop | The ball settles on its entry side, as close to the centre as the pole allows, fully below the surface |

On the ball actor, `SetCups(TArray<FOpenFairwayCup>)` registers surface positions (world cm) and normals. The
physics plugin does not depend on course actors or terrain implementations. Invalid positions are ignored and invalid
normals default to world up. `SetCupLocations` remains a position-only wrapper using world-up normals, and
`GetCupLocations` remains available for existing consumers.

A grounded ball's motion segment is intersected with all apertures before the solid-green sweep; the earliest entry
wins, independent of registration order. Entry retains its height, velocity and spin rather than snapping onto a
horizontal cup plane. Capture limits and landing tests use the same local frame. A fast crossing bypasses terrain
collision until it exits or is holed. Ordinary physics integrates the unconsumed part of the exit timestep.

A captured ball stops and sinks over `HoleDropSeconds` (0.35 s); `OnBallHoled` fires at capture and `OnBallAtRest`
after the drop. `bHoled` is set and `bOverCup` is cleared. Below-ground recovery skips holed balls.
`PlaceAt` and `Launch` reset crossing state; registering new cups cancels an active crossing.

Diagnostics for the latest crossing include entry speed, capture limit, chord offset, spin, world exit velocity (m/s),
and `CupMaxHeightM` (ball-bottom height above the cup's tangent plane, not the launch-relative shot apex). The entry
log also records the surface normal. No height cap is applied. The plane-based height is a local diagnostic; terrain
far beyond the cup can depart from that tangent plane.

`UOpenGolf.Physics.CupContact` checks energy and convergence with smaller substeps on flat and sloped frames.
`UOpenGolf.Range.CupThirtyFoot` launches the actual ball actor from exactly 30 horizontal feet at 8.6 mph on a flat
stimp-10 fixture and the saved green, with straight and recorded replay angles. It reports both an unadjusted replay
(which misses the hole) and aimed crossings. These are reproducible model measurements, not real-world calibration.

The course kit draws the cup and lifts the flag (`UOpenGolfCupComponent`, see
[Open Golf Course](../../OpenGolfCourse/Docs/Reference.md#cup)).

### Green tests

| Test | What it checks |
| --- | --- |
| `UOpenGolf.Physics.GreenReference` | 21 historical-profile golden values from the harness (putts at stimp 8/10/11/13, a lofted putt, flop, bucketed wedges with and without bite, 2–3° green slopes, sloped fairway and rough), then the reference test bands: `PuttRollRegressionTests`, `LandingBucketRegressionTests`, `GreenSpinBackRegressionTests` (flat and 10° slopes) and green-versus-fairway reverse tangent speed; the current profile keeps slope direction |
| `UOpenGolf.Physics.SlopeRollout` | Level ground and slopes below the hold angle keep a resting ball still; steeper slopes roll it with resistance; rough holds a slope that rolls a ball on green (current and historical profiles) |
| `UOpenGolf.Physics.GreenRollout` | After 0–6,000 rpm landings, a level stimp-10 green rolls a ball at 2–12 m/s at least as far as fairway; faster greens roll farther; the stimpmeter distance holds; the historical reference keeps the ported green |
| `UOpenGolf.Physics.GreenStimp` | Green Stimp Feet reaches predictions: the default matches the headless putt, a faster green rolls it farther, out-of-range stimps clamp, fairway unchanged |
| `UOpenGolf.Physics.Fixtures` | Includes `wedge`, `putt` and `checked` on Green |
| `UOpenGolf.Physics.CupCapture` | Port of ogolf `CupCaptureTests`: regulation size, the 1.64 m/s centre and zero rim limits, slow drops and fast lip-overs, near-rim lines, stopped and leaning balls, dunks, the pin height gate and the drop point |
| `UOpenGolf.Range.HolePlay`, `UOpenGolf.Range.GreenContour` (game module) | The ball actor on the saved hole: a firm putt rolls over the cup, an off-line putt misses, a dying putt drops; the contoured green's contact normals, a resting ball on its steepest part, and a putt breaking downhill |
| `UOpenGolf.Range.RolloutSurfaces` (game module) | The same path rolls farther tagged green than tagged fairway |

Regenerate the green golden values with
`dotnet run --project Plugins/OpenFairwayPhysics/Tools/ReferenceHarness -c Release -- <path-to-ogolf-physics> greens`
and copy them into `Private/Tests/GreenReferenceTest.cpp`.

### Green port status and gaps

Ported in full from ogolf-physics `e6d793a`: the green preset and spin-back window,
`GreenSpeed` (stimp range, stimpmeter speed and solve), the profile stimp and its
per-surface application, landing buckets, slope hold and drive in `StepGround`, and
the green, putt, spin-back and slope regression tests. Remaining:

- The grass drag deliberately differs (above); no measured green-rollout reference
  exists for either value. `GreenRollout` checks only that a green out-rolls fairway
  after full-shot landings.
- JSON profiles (`"GreenStimpFeet"` in a profile file) are not ported; set
  `GreenStimpFeet` on the ball or profile.
- `SurfaceZone` area overrides are not ported; declare greens by material or tag.

## Flight model

Gameplay flight (`FBallPhysicsProfile()`, `EFlightModel::SpinRatioV1`, since 2026-11-18) is Newton's
law with gravity, drag and Magnus lift on the ball's velocity through the air, and the Smits and
Smith spin decay, integrated with coupled RK4 (`FFlightProfile::bUseRungeKutta4`) at 120 Hz. The
coefficients depend only on the air flow, never on the club, the launch angle or a launch regime:

- **Lift** is a curve in the spin ratio W (surface speed over air speed): zero without spin, rising
  through 0.18 at W 0.10, 0.29 at 0.22 and 0.35 at 0.50.
- **Drag** is a curve in W, from 0.187 at low spin to 0.361 at high spin (spin-induced drag), plus
  0.68 of the measured rise of a modern ball's drag below Re 100k (the drag crisis).

The table is in `Private/OpenFairway/SpinRatioData.h`. Both curves were fitted, rising with spin and
smooth, to the launches of three reference sets in the
[local reference data](../../../Assets/Data/README.md#where-the-data-lives), weighted tm 2 : lm 1 :
fs 0.5. Drag is held at or above 0.17, the measured minimum of a modern ball, and lift below the
launch data follows the measured lift of a modern ball (Lyu et al. 2018). The fit chose the
drag-crisis share itself, and its lift matches that measured lift at higher spin too.
`Assets/Scripts/flight_fit.py` refits the table from the local data and prints it. See
[CarryAudit.md](CarryAudit.md) for the analysis and the rejected alternatives.

Engine minus reference in Unreal (`carry_audit.py`, 2026-11-18), bias / mean absolute error:

| Reference | Shots | Speed | Carry, yd | Apex, ft | Landing, ° | `LegacyCalibrated()` carry |
| --- | ---: | --- | --- | --- | --- | --- |
| tm, tour averages | 23 | 86–167 mph | +0.3 / 2.3 | +3.6 / 4.7 | −1.3 / 1.7 | +10.1 / 10.1 |
| lm, launch-monitor export | 124 | 36–121 mph | −2.2 / 2.2 | −0.1 / 0.6 | +1.3 / 1.4 | −1.0 / 4.1 |
| fs, trajectory calculator | 13 | 109–175 mph | −3.8 / 3.8 | −0.4 / 1.2 | | +2.9 / 3.4 |

The references disagree with each other by several yards for the same launch: fs carries 8–10 yd
farther than tm at its average launches, and lm's model 2–3 yd farther than tm's. The fit leans to
tm. Outside the fitted range (Re 35k–215k, W 0.06–0.8; about 36–175 mph) the curves are held at
their ends and `FFlightAerodynamicsSample::bOutsideMeasuredCoverage` is set.

`FBallPhysicsProfile::LegacyCalibrated()` keeps the previous gameplay flight: the inherited drag
polynomial and lift curve with the launch-regime overrides, spin-drag, high-launch and mid-spin
corrections and the [flight corrections](#flight-corrections), on the ported Euler step. The
sections from here to [Wind and height](#wind-and-height) describe that model, which
`HistoricalReference()` and the golden fixtures still use; SpinRatioV1 reads none of it. A bare
`FFlightProfile()` is also legacy, so the stimpmeter conversion and the air drag on a rolling ball
are unchanged. `ProV1Reference()` is a comparison only: digitized drag and lift of one modern ball
(Lyu et al. 2018, 2020, `ProV1Data.h`), whose drag was measured near 2250 rpm; applied to every
spin it carries fast shots 28 yd too far against lm.

## Ball profile and launch regimes

`AOpenFairwayBall::Profile` (`OpenFairway::FBallPhysicsProfile`) holds the calibrated
defaults: drag scale 1.01, green stimp 10 ft (set from Green Stimp Feet at launch), the flight/bounce/rollout constants, the
landing responses and the launch-regime overrides. The regime table's drag and lift scales reach only
the legacy flight (see [Flight model](#flight-model)); its contact multipliers and the landing
responses apply to every flight model. Current defaults interpolate
override values between speed/launch/spin anchors, preserving the most-specific
lookup and neutral fallback at each anchor. Landing responses are interpolated
over launch/spin too. The regime and landing bucket are descriptive categories;
logs identify the resolved override as `interpolated`. Changes apply from the next
launch. See [Lookup and continuous calibration](#lookup-and-continuous-calibration) for anchors.

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
is `ShotRegimeKey` in [BallProfile.cpp](../Source/OpenFairwayPhysics/Private/OpenFairway/BallProfile.cpp).

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

Examples from the replay shots:

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
impact spin. The current profile reads launch backspin (total spin on `HistoricalReference()`),
so a sidespin-heavy launch is not bucketed as a checking wedge. Only green and sand landings use
the bucket's bite now: turf reads its landing loss from the impact itself (see
[Turf rollout calibration](#turf-rollout-calibration)), so on turf the bucket
is a label only. The labels describe a separate first-impact response table:

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

### Replay shots and the low-launch lift correction

In the game's **Test shots [P]** dropdown, select a low wood, mid iron, driver, iron,
wedge, chip, high flop or putt, or a recorded launch-monitor shot. **Hit selected shot**
launches it; **H** repeats the selection. The panel shows each preset's inputs. The ten
presets belong to the game's launch-monitor replay catalog, not to the physics plugin's
runtime dependencies.

| Replay shot | Gameplay carry (SpinRatioV1) | `LegacyCalibrated()` carry |
| --- | ---: | ---: |
| Mid iron, 75.7 mph / 20.78° / 4463 rpm | 85.22 yd | 85.94 yd |
| Low wood, 114.5 mph / 6.95° / 1933 rpm | 121.80 yd | 121.55 yd |

The rest of this section concerns the legacy flight. The low wood previously carried **128.13 yd**.
That model now sets
`LowLaunchLiftRecoveryMax = 1.0` instead of 1.08, removing the extra empirical
boost while retaining ordinary Magnus lift and the corrected spin decay. This
produces **121.55 yd**, 15.03 ft apex and 2.741 s hang time. The boost only operated
below 9.5° launch, further weighted by Reynolds number and spin ratio. The 20.78° mid
iron and other shots at or above 9.5° are unaffected; low-launch shots can shorten.

The diagnosis isolated regime interpolation (−0.66 yd before this change), spin
decay (+4.96 yd with hard lookups), and the low-launch boost (+6.58 yd with the
corrected spin model). The mid-spin boost had no effect on this shot. No regime
rows, boundaries or global drag/lift multipliers were changed. This is a targeted
calibration correction, not proof that all low-launch shots match measured flight;
the [shot library](#shot-library) is the reference for that. See
[Does the regime limit rollout?](#does-the-regime-limit-rollout) for rollout differences and the
controlled surface experiment.

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
**0.011** (0.0009 since 2026-10-18; see [Grass drag and the ported value](#grass-drag-and-the-ported-value)), versus the ported **0.0017** on
fairway and **0.00245** on fringe. This coefficient
enters speed-dependent translational drag and spin damping. The green also uses
stimp-derived rolling resistance and excludes the fairway's spin multiplier from
that resistance. A 10 ft stimp check validates low-speed roll in this model;
it does not validate the whole high-speed post-impact response.

The distances in the rest of this section were measured with the ported turf, before the
[turf recalibration](#turf-rollout-calibration); on the current flat fairway the
low wood rolls **45.0 yd to 166.5 yd total** (2026-11-17).

The corrected shot on a single flat fairway gives **37.69 yd roll / 159.24 yd total**,
whereas the mixed range reproduced **31.92 / 153.47 yd** with that green. Replacing only the green
with fairway, keeping the fringe, gives **36.71 / 158.26 yd**. Thus the original
range loses about 5.77 yd versus all fairway; this is a surface-response effect,
confirmed with identical actor and geometry. Compare
equivalent surfaces before changing regime calibration to compensate for the
range layout. The distances above are from that controlled surface experiment.

The test range assigns **Fairway to its 34 m fairway strip**, including the former
green/fringe area, through the `MI_Fairway` physical material; the side strips beyond
±17 m play as Rough. With the ported turf the low wood rolled 37.69 yd to 159.24 yd total down
the line and rested on Fairway. The ball reads each contacted mesh's physical material (or surface tag);
no surface is forced inside the physics solver or replay path. Other surface presets remain available for scenes that
assign them. The rollout regression test explicitly recreates the old mixed
assignments in a disposable test world to preserve that comparison.

## Flight corrections

Two corrections to the inherited aerodynamic fits are on in `LegacyCalibrated()` (the gameplay
flight before 2026-11-18). `HistoricalReference()` turns both off, so the golden fixtures are
unchanged. The gameplay flight model does not use the inherited fits.

| Correction | Profile field | Problem | Effect |
| --- | --- | --- | --- |
| High-Re drag blend | `Flight.HighReCdBlendStart = 175000` | Drag stepped from 0.186 to the 0.2 cap at Re 200k (about 162 mph at 75 F) | Drag eases into the cap from Re 175k; no shot launched below 141 mph moves |
| Low-Re lift bridge | `Flight.bBridgeLowReLift = true` | The 60k and 65k lift fits clamp to zero for spin ratios 0.1-0.2, so lift vanished at about 43-54 mph | Re 50k-70k interpolates the 50k and 70k fits, so slow shots keep their lift |

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

- **Height:** in flight, the ball centre's height above the launch ground, not above the ground
  beneath the ball. The core has no world access. A ball in flight below its launch ground gets no
  wind. A ball on the ground meets the wind one radius above its own lie, wherever that lie is, so
  rolling up or down a slope does not change its wind; under the profile that is none.
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
  reads the height from `State.Position.Y`; `GolfPhysics::StepGround` uses the ball radius.

### Headwinds

The figures in this section were measured with the legacy flight (`LegacyCalibrated()`, the gameplay
model until 2026-11-18). The gameplay [flight model](#flight-model) passes the wind tests' direction
and size checks but has not yet been compared with the published wind sets below.

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
  ([Golf.com](#references)).

Low-launch shots are close to wind-neutral even with the published coefficients, so "always
shorter" is not a law of physics for them. Still, this model:
- gives them at least 2.5 times the larger published gain;
- loses about 20–40% less carry than the published sets for a driver in a strong headwind;
- loses less than the 20 mph rule of thumb (−12% against −14%).

At 10 mph its driver loss matches the 12 yd rule.

**Cause.** In this model, the drag coefficient at the driver and low-wood Reynolds numbers
(1.4–1.7 × 10⁵) is about 0.20. The Smits & Smith spin terms give about 0.25 at the same spin.
The inherited fits pair that low drag with low lift, so calm-air carries matched the reference
data they were fitted to in ogolf-physics.
In wind, the extra lift then outweighs the extra drag by more than it should.

**Status.** The 2026-11-18 [flight model](#flight-model) replaced these fits for gameplay, with drag
at 0.20–0.24 for a driver's spin and spin-induced drag rising above it. What remains is to compare
it with the published wind sets and add wind assertions to `UOpenGolf.Physics.Wind`: a driver
loses about 25–35% of its carry into 30 mph, as the published sets do, and a low wood stays within
±5 yd of calm.

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
| `bLanded`, `bAtRest` | The shot has landed; the ball has come to rest |
| `TerminationReason` | `Rest`, or `SafetyStop` when the ball ran past `MaxShotSeconds` or left the playable area |
| `ElapsedS` | Simulated time since launch |
| `bHoled` | The ball dropped into a cup (see [Cup and hole-out](#cup-and-hole-out)) |

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
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Test -Filter UOpenGolf.Physics
```

```bash
bash Assets/Scripts/ue.sh test --filter UOpenGolf.Physics   # add --engine-root PATH if UE_ROOT is unset
```

| Test | What it checks |
| --- | --- |
| `UOpenGolf.Physics.Core` | Air properties (including humid air and station pressure), spin parsing, launch vectors, regime keys, landing buckets, friction, rest and rebound rules, green speed |
| `UOpenGolf.Physics.Fixtures` | 21 historical-profile shots against unchanged C# golden values: distances (0.1 yd), apex (0.1 ft), hang time (0.01 s) and landing bucket; current defaults also checked for finite, completed shots |
| `UOpenGolf.Physics.FlightCorrections` | Analytical spin decay, mirrored directions, landing spin, anchor values, interpolation bounds, boundary continuity, the high-Re drag blend and the low-Re lift bridge |
| `UOpenGolf.Physics.Wind` | Calm air unchanged; head-, tail- and crosswind direction and size; a ball moving with the air feels gravity only |
| `UOpenGolf.Physics.WindProfile` | The log profile's share at 1, 2, 30 and 100 m and its cap above 100 m; no wind at or below the roughness length, below the launch ground or at a non-finite height; uniform wind for settings that cannot describe a profile; `Resolve` copies the profile; a flight step reads the ball's height |
| `UOpenGolf.Physics.WindHeight` | Calm air is identical with and without the profile; a tailwind changes a low punch's carry less than uniform wind; a high wedge keeps more of its crosswind drift than a low punch; reports driver, wedge and punch carry and drift |
| `UOpenGolf.Physics.Sand` | A low wood rests in sand; a shallow full-shot landing stays within 3 m where fairway lets it run; a steep landing stops within 1 m; a grazing contact keeps more pace than a shallow landing, and the plow never reverses the ball; a ball rolling in at 4 m/s stops within 2 m; the profile's plow ratio and rolling friction apply, and the reference keeps the ported sand |
| `UOpenGolf.Physics.CupContact` | Cup-lip crossings on flat and sloped frames end, never gain energy, converge with smaller substeps (outcome, exit velocity, time), exit above the surface and return their unconsumed time |
| `UOpenGolf.Physics.ShotInterrupt` (game module) | A shot still moving at its time limit ends as a safety stop, before the fall-through limit and with no invented landing; re-teeing a shot in flight finishes it as interrupted and keeps its progress |
| `UOpenGolf.Physics.ObstacleResponse` | Face-on, glancing, edge and flat-top obstacle contacts; tagging by component or actor |
| `UOpenGolf.Physics.CanopyResponse` | Canopy speed loss by path length (step-size independent), bounded seeded deflection, tagging; canopy is not an obstacle |
| `UOpenGolf.Physics.SurfaceResolution` | Resolution order: component tag, hit or component physical material, actor tag, default; plain physical materials are ignored |
| `UOpenGolf.Physics.GreenReference`, `SlopeRollout`, `GreenRollout`, `GreenStimp` | Greens, putting and slopes; see [Green tests](#green-tests) |
| `UOpenGolf.Physics.VisualRoll` | Roll angle is distance over radius, the top moves with the motion, the per-frame cap, no motion and slopes |
| `UOpenGolf.Physics.FlightModel` | The gameplay flight: lift and drag table values, lift and drag never fall with spin, drag never below 0.17, the low-Reynolds drag rise and its continuity, lift within 0.025 of the measured modern-ball lift at W 0.18 and 0.32, coverage flags, no dependence on launch angle, global scales or the regime table, calm heading invariance; eight reference carries (tour averages, calculator and launch-monitor launches from 102 to 167 mph) within 6 yd; the Pro V1 comparison model against its published figures |
| `UOpenGolf.Physics.FlightIntegration` | The RK4 flight step: exact vacuum ballistics, spin kept without torque, zero spin stays zero, drag slows the ball, mirrored side spin mirrors the flight |
| `UOpenGolf.Physics.ClubLabelIndependence` | The same launch under another club name gives the same carry, total and apex |
| `UOpenGolf.Physics.FlightDiagnostics` | The 75.7 mph mid iron and 114.5 mph low wood replay launches under `LegacyCalibrated()`: production and diagnostic flight agree; pinned carries for the historical profile, the isolated spin-decay experiment and the corrected wood; regime keys; a mirrored heading keeps carry; historical, decay, interpolation, lift/drag and timestep diagnostics |
| `UOpenGolf.Physics.RolloutRegression` | On every surface, at 0.1–45 m/s with topspin, no spin or backspin, the ground step never adds energy and leaves a rolling ball with no slip; recorded and fixture shots converge within 1% (or 0.25 yd) between 240 and 480 Hz; spin about the normal leaves an impact unchanged and a pivoting ball can rest; air drag still acts on a ball on the ground, the same rolling state above, below or level with the launch point meets the same wind, and environments resolved for different launches give the same ground step; on fairway, two launches that arrive with the same velocity and spin leave the same way, and the landing keeps less at steeper angles and with backspin and nearly all of a skim; without friction backspin takes nothing, and at 2 and 8 m/s with 2000 and 8000 rpm a contact keeps 99.9% of its speed at 0.1 mm/s of normal speed and never more as the normal speed grows to 2 m/s; the tilted-axis 107.8 mph shot rolls at least 2.5× farther than before; ground-launched shots contact at t = 0 and come to rest; at 60, 80 and 100 mph a shot struck along the ground at 0° goes farther than one launched at 0.1°; apart from that pair, no 1° launch step changes total by more than 15% at 60, 80 or 100 mph, nor any 0.1° step near 0° by more than 3%; 5 and 10 m/s launches stay within 3% per 0.1° across 0°; 109 mph / 13.8° / 4242 rpm releases 8–16 yd and 78 mph / 23.5° / 7528 rpm holds within 0.5–5 yd; fairway is slower than the green on a stimpmeter and rough slower again; green and sand keep the ported landing and have no struck drag, and all five turf surfaces use the impact-based one; the struck drag is none below 12 m/s of launch speed and all of it from 18 m/s |
| `UOpenGolf.Calibration.Rollout` (opt-in) | Not a check: fits the turf landing loss and ground resistance to the local launch-monitor shots that leave the ground (stage 1), then the struck-along-the-ground drag scales to the topped shots (stage 2). Writes every candidate, with its roll error by launch class, continuity, stimp, slope hold and the acceptance items it misses, to `Saved/ShotLibrary/calibration.csv`. Reports the catalog values and both stages' best; see [Turf rollout calibration](#turf-rollout-calibration) |
| `UOpenGolf.Physics.ShotLibrary` | Replays every session in `Assets/Data/ShotLibrary/` and in the local reference data (when present), each shot in its session's air, with every profile in `MakeShotLibraryProfiles()`, and writes `Saved/ShotLibrary/results.csv`: distances, landing angle, speed and spin, first-impact speeds, bounce count, ground distance, whether the shot rested, the air, and the flight's Reynolds and spin-ratio range and time outside the fitted range. Checks that every shot parses and stays finite, that the current profile brings each to rest, and that every RK4 carry converges within 0.1 yd of a 960 Hz run; not reference accuracy |
| `UOpenGolf.Range.Shot` (game module) | A drive hit in the running range lands within 1 yd carry and 0.5 yd offline of the headless prediction, and stops within 2 yd total and 2 yd offline (headless 243.99 / 261.59 yd with the 2026-11-18 flight; range 243.92 / 261.60 yd). The roll-out meets real contours the flat prediction lacks: the back fairway falls to the right there (about 1.2° under the 2026-11-18 roll), which moves the live ball about 1 yd right of the flat roll, so the test also requires any roll-out difference to point downhill |
| `UOpenGolf.Range.RecordedShots` (game module) | The four recorded launches that exposed the rollout faults (67.5 mph at −0.14°, 57.7 mph at 0.08°, 100.2 mph at 0.78° and 107.8 mph at 8.4° with an 85° spin axis), hit on the range: each comes to rest with the headless carry; totals are reported (81.3, 41.6, 79.7 and 105.5 yd on 2026-11-18, against 87.1, 41.5, 80.0 and 105.6 yd on flat fairway; the first, a topped shot, ends in rough on the range) |
| `UOpenGolf.Range.FlightParity` (game module) | Six launches from full shots to a topped shot and a sub-step flight: the live ball's carry within 0.25 yd of the headless prediction at 30 and 144 fps, and identical at 60 fps with 0.1 s stalls |
| `UOpenGolf.Range.RecordedContactParity` (game module) | Five recorded launches on every surface: the live ball's carry and total within 0.25 yd of the headless prediction; a roll across a fairway boundary into rough, green and sand traces both surfaces and ends on the one under the ball |
| `UOpenGolf.Range.FrameRates` (game module) | Fixed-step outcomes across 30/60/144/5 FPS and 250 ms hitches, including the twelve-step catch-up cap |
| `UOpenGolf.Range.ReplaySelection` (game module) | Selected/repeated replay reaches the ball; the low wood carries within 1 yd of its flat-ground prediction (121.14 yd on the range against 121.80 yd, 2026-11-18) |
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
revision in the source headers when you port newer ogolf-physics changes. Pass `greens` instead
of shot names for the [green cases](#green-tests). A shot can also be `putt=<mph>[,<vla>]`, and
`@Surface` can take `:stimp=<ft>`, `:slope=<deg>` (downhill along the target line; negative is
uphill) and `:neutral` (no landing-bucket bite), e.g. `putt=6@Green:stimp=11:slope=-2`.

### Shot library

The golden fixtures prove port parity only. To see how the physics compare with reference
flights, `UOpenGolf.Physics.ShotLibrary` replays every session through `ShotSimulator::Simulate` on
fairway, each shot in its session's air (75 °F, dry, sea level unless the session says otherwise),
with every profile in `MakeShotLibraryProfiles()`. Sessions come from
[`Assets/Data/ShotLibrary/`](../../../Assets/Data/README.md) (uopengolf's own recordings) and, when
present, the local reference data outside the repository: lm, tm and fs (see
[Flight model](#flight-model)). Run the reports from the repository root after building:

```powershell
python Assets/Scripts/shot_compare.py      # roll and total, with the accuracy gates
python Assets/Scripts/carry_audit.py       # carry, apex and landing per reference session
```

`shot_compare.py` prints engine minus reference per shot for carry, total, roll, apex and landing
angle, then summaries per profile, roll by launch class (the table in
[Turf rollout calibration](#turf-rollout-calibration)), errors by ball speed, and current-profile
rollout by spin bin and landing angle. It grades only shots with a reference total (lm); tm and fs
give flight only and are reported by `carry_audit.py`. It also shows the change since the last run
with different results, so rerun it after every physics change. Shots are read at run time, so
adding a session needs no rebuild: import a launch-monitor export with
`python Assets/Scripts/monitor_import.py <export.csv>`. On Linux, use `python3`.

`recorded_20261113` holds the launch inputs of the 36 range shots recorded on 2026-11-13 (copied by
`Assets/Scripts/recording_import.py`, with the original JSON files). It has no reference results, so
it is replayed and must come to rest, but is not compared; the engine's own recorded distances are
not treated as truth.

A run through `shot_compare.py` writes `Saved/ShotLibrary/results.manifest.json`: a hash of the
physics and game sources, the library and the physics binaries at the time of the run, and of the
results. `--check-baseline <older results.csv>` then fails the run unless the results are fresh
for that hash; every current-profile shot rests with finite values; roll and total mean absolute
error do not grow; the worst roll error shrinks by at least 25%; and no launch class's roll error
grows by more than 0.5 yd. The baseline, `baselines/pre_contact_20261116.csv` in the local data,
is lm. With the legacy flight the turf fit passed all of them (2026-11-17): roll error
3.42 → 1.23 yd, total error 6.02 → 3.98 yd, worst roll 34.3 → 7.8 yd, and every launch class
improved (ground-launched 22.2 → 0.8 yd). The first 2026-11-17 fit had failed three, all through the
six topped shots (see [Topped shots](#topped-shots)). With the 2026-11-18 flight one gate fails:
mid-launch shots above 4000 rpm roll 2.3 yd short of lm (1.1 → 2.3 yd error), since balls now land
about 1.3° steeper and slower than in lm's model and the turf was fitted to the legacy landings;
roll error overall is 1.8 yd and total error 3.9 yd. Nothing in the report or the gates reads a
club: shots.csv keeps the club that was selected as `reported_club`, an unverified label.

To evaluate a physics change before making it the default, put it behind a profile field
that defaults to today's behaviour and add a named profile to `MakeShotLibraryProfiles()`
in `Private/Tests/ShotLibraryTest.cpp`. The report lists it beside `current` and
`historical`, and `--profile <name>` shows its per-shot table.

## Differences from ogolf-physics

- No exceptions: an invalid contact normal falls back to up, and launches are validated
  (`FLaunchData::IsValid`) before simulating.
- Single-precision math, as in C#. The port uses `MathF.PI`'s value, not the engine's
  double-precision pi, to stay bit-close.
- Out of bounds is 1000 m from the launch point (ogolf-physics uses the world origin).
  A ball whose centre ends up 0.5 m or more below the ground under it is lifted back onto
  that ground, as ogolf-physics' `TryRecoverToGround` does; the port measures from the
  ground found by a trace from 2 m above the ball, not from world height 0. A ball still
  100 m below its launch point stops.
- The ball actor spends the remainder of a landing step moving after impact, with
  another sweep to detect intervening geometry. This now matches the headless loop's
  contact timing closely enough to avoid selecting a different subsequent bounce.
- The gameplay flight is not ogolf-physics' aerodynamic model: it uses the spin-ratio curves of
  [Flight model](#flight-model) and an RK4 step. `LegacyCalibrated()` keeps the ported model with
  the current spin decay, neutral low-launch lift recovery, continuous calibration, the high-Re drag
  blend and the low-Re lift bridge; use `HistoricalReference()` when comparing golden fixtures.
- Wind, humidity and station pressure are additions; ogolf-physics has calm, dry air.
- The green's grass drag is 0.0009 instead of the ported 0.011; see
  [Grass drag and the ported value](#grass-drag-and-the-ported-value).
- Impacts and ground contact read axis-aware spin. The five turf surfaces read their landing loss
  from the impact (in place of the first-impact retention, its ramp, the landing bucket's bite and
  the rollout-bounce retention), carry a speed-squared grass drag ogolf-physics does not have, and
  are fitted to launch-monitor rollout. A ball on the ground also feels air drag, a launch along or
  into the ground contacts it at t = 0, and a full shot struck along the ground rolls with its own
  grass drag; see [Turf rollout calibration](#turf-rollout-calibration).

### Port status

Checked against ogolf-physics `e6d793a` (2026-10-29):

- **Identical under `HistoricalReference()`:** every constant and formula in `StepFlight`, `ResolveImpact`,
  `ApplyLandingBite`, `StepGround`, `SpinFrictionMultiplier`, `CanRest`, `LaunchSpin` and the
  aerodynamic model; the seven surface presets and their bounce settings; `GreenSpeed`; the
  regime and landing-response tables and bin edges; spin parsing and launch vectors; the
  headless shot loop (`PhysicsAdapter.SimulateShotInternal`), including sloped ground and the
  first-impact spin-back diagnostics; the ball's contact, landing, wall and ground-probe rules
  and below-ground recovery.
- **Deliberately different** (`HistoricalReference()` restores the reference): the gameplay flight
  model and its RK4 step (2026-11-18), flight spin decay, low-launch lift recovery, continuous calibration, the high-Re drag blend, the low-Re
  lift bridge, the sand plow, the green grass drag, axis-aware contact spin, the coupled ground
  step, contact at t = 0 for a launch into the ground, the turf landing law and fit, and the
  struck-along-the-ground drag (2026-11-17). Wind, humidity, station pressure, air drag on the ground and turf form drag are
  additions.
- **Replaced:** Godot's `LieSurfaceResolver`, `SurfaceZone` and `GridMapSurfaceCache` by
  physical materials and tags; `PhysicsCoreBridge` and `PhysicsLogger` by native types and
  `UE_LOG`.
- **Not ported:** JSON ball profiles, `SimulateCarryOnly`, the initial and peak aerodynamic
  diagnostics, `CdMin` and `PennerLowEnergyThreshold` (unused by the reference's physics), and
  the reference's argument-validation exceptions (the port clamps or falls back instead).

## Known gaps

- JSON ball profiles (`BallPhysicsProfile.FromJson`) are not ported; change `Profile` in C++.
- `SurfaceZone` area overrides, Terrain3D height queries and the lie resolver are not ported;
  use physical materials (per material or Landscape layer) or surface tags.
- Wind has no gusts, and in flight its height is measured from the launch ground, not the terrain below
  the ball (a ball on the ground uses its own lie).
- Headwinds cost too little carry. A driver loses about 20–40% less than published aerodynamics
  into 30 mph, and low-launch shots gain 5–14 yd. See [Headwinds](#headwinds).
- No shot camera, tracer styling or HUD here (the game module supplies those).
- Packaged builds are only start-checked headless; ball flight in a rendered packaged game
  has not been reviewed on either platform.
- Flight (see [Flight model](#flight-model)): the reference sets disagree with each other by several
  yards for the same launch, and the fit leans to the tour averages (tm). Against them carry is off
  by 2.3 yd per shot and apex runs about 4 ft high (more for the men's driver); against the
  launch-monitor export (lm) carry is 2.2 yd short on average. The flight table is fitted, not
  measured: only its low-Reynolds drag rise and its low-spin lift come from a measured modern ball.
- Roll was fitted to lm with the legacy flight. The gameplay flight lands about 1.3° steeper and
  slower than lm's model, so roll runs 1.7 yd short of lm (2.3 yd for mid launches above 4000 rpm,
  past the 0.5 yd gate) until the turf is refitted to the new landings.
- Live and headless contact differ for a grazing landing: the live sweep finds a contact only once the
  ball is 0.5 mm into the surface, so a contact the headless loop resolves near the end of a step
  can land in the next one, and the live ball then moves a whole step before its first ground step.
  On sand, where a rolling ball loses about 33 m/s², a 113.9 mph / 0.6° launch that touches down
  twice ends 0.31 yd farther live, and `UOpenGolf.Range.RecordedContactParity` fails that one case
  (0.25 yd allowed). The fix is a ground step for the rest of an impact step, in both loops.
- Matching lm's six topped shots takes a rule that reads how the shot started, so at full speed
  total distance steps by about 45% between a launch at 0° and one at 0.1° (see
  [Topped shots](#topped-shots)). lm's own results step the same way.
- Still empirical: the restitution spin cut, the later-impact restitution cutoff and scale (the
  turf grip bound inherits its step), the green and sand landing (retention, ramp and landing-bucket
  bite), and the struck-along-the-ground drag, which reads how the shot started. No roll reference is
  faster than 121 mph ball speed, and none was hit on rough or the other turf surfaces. See
  [shot library](#shot-library).
- Green gaps: see [Green port status and gaps](#green-port-status-and-gaps).

## Plugin structure

```text
OpenFairwayPhysics.uplugin
Source/OpenFairwayPhysics/
  Public/OpenFairway/       Core (Core module only): Profiles (flight, bounce, rollout constants),
                            GolfPhysics (steps, impact, aerodynamics), Surfaces (catalog, green
                            speed), BallProfile (profile, regimes, landing buckets), ShotSimulator,
                            WindProfile, CupCapture
  Public/OpenFairwayTypes.h Blueprint types: surfaces, ball state, launch, shot result
  Public/OpenFairwayBall.h  Ball actor (Chaos sweeps and traces)
  Public/OpenFairwayPhysicalMaterial.h  Physical material that declares a surface
  Private/OpenFairway/      Core implementation, including FlightAerodynamics, WindProfile and the
                            flight tables (SpinRatioData.h; ProV1Data.h for comparison)
  Private/OpenFairwayBall.cpp  Ball actor
  Private/Tests/            Automation tests: core, golden fixtures, flight corrections, wind,
                            greens and slopes, sand, cup, obstacles and canopy, visual roll,
                            rollout regression, the shot library and its calibration
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
  into slower flight still needs validation.
- A. J. Smits and D. R. Smith, "A new aerodynamic model of a golf ball in flight," in
  *Science and Golf II: Proceedings of the 1994 World Scientific Congress on Golf*,
  A. J. Cochran and M. R. Farrally (eds.), E & FN Spon, London (1994), 340–347. Cited
  through Nathan's paper above. Source of the spin-decay law and of the spin-dependent drag and
  lift terms used in the [Headwinds](#headwinds) comparison and the 2026-11-18 flight analysis.
- J. Lyu, J. Kensrud, L. Smith and others, "Aerodynamics of Golf Balls in Still Air,"
  *Proceedings* 2(6), 238 (2018), [doi:10.3390/proceedings2060238](https://doi.org/10.3390/proceedings2060238),
  CC BY 4.0: drag and lift of modern balls; source of the low-spin lift of the
  [flight model](#flight-model) and of the comparison profile's lift.
- J. Lyu, J. Kensrud and L. Smith, "The reverse Magnus effect in golf balls," *Sports Engineering*
  23, 7 (2020), [doi:10.1007/s12283-020-0318-1](https://doi.org/10.1007/s12283-020-0318-1),
  CC BY 4.0: drag of a modern ball against Reynolds number (the drag crisis used by the flight
  model) and signed lift at low speed.
- F. Alam and others, "A study of golf ball aerodynamic drag," *Procedia Engineering* 13 (2011),
  226–231, and H. Chowdhury and others, "A study of dimple characteristics on golf ball drag,"
  *Procedia Engineering* 147 (2016), 87–91: drag of non-spinning commercial balls (background).
- World Meteorological Organization,
  [Guide to Meteorological Instruments and Methods of Observation (WMO-No. 8)](https://www.weather.gov/media/epz/mesonet/CWOP-WMO8.pdf),
  seventh edition (2008; NOAA-hosted copy), Part I, Chapter 5 "Measurement of surface wind":
  - the 10 m standard height over open terrain;
  - the annex table of Davenport's terrain classes as adapted by Wieringa, whose "open" class
    has z0 = 0.03 m;
  - the correction of readings to z = 10 m and z0 = 0.03 m.
- A. Rice, [How weather conditions affect your golf ball](https://golf.com/instruction/weather-conditions-effect-golf-ball/),
  Golf.com (24 June 2019): from a year-long launch-monitor study, a 300 yd drive carries 259 yd
  into a 20 mph headwind.
- M. Harman, [Editorial – Wind, Temperature and The Open](https://www.usgtf.com/?p=29996),
  USGTF (1 August 2021): a driver loses about 12 yd of carry into a 10 mph headwind and gains
  about 9 yd downwind.
- NASA Glenn, [Viscosity](https://www.grc.nasa.gov/www/BGH/viscosity.html):
  Sutherland's temperature-dependent viscosity law used in the air model.

## License

MIT, as part of uopengolf. The equations and calibration come from ogolf-physics
(`addons/openfairway`, MIT), the maintainer's own project; see
[THIRD_PARTY_NOTICES.md](../../../THIRD_PARTY_NOTICES.md).
