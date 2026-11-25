# OpenFairway Physics

Native C++ golf-ball physics for Unreal Engine 5.8, and an Unreal ball actor that uses Chaos only for
contact queries. Paths are relative to the plugin root unless stated otherwise. How well it matches
reference shots is in [Accuracy](Accuracy.md); validation runs are in the
[phase status](../../../Docs/PhasesStatus.md).

## Contents

- [Overview](#overview)
- [Quick start](#quick-start)
- [Units and frames](#units-and-frames)
- [Launch](#launch)
- [Air and wind](#air-and-wind)
- [Flight](#flight)
- [Impact](#impact)
- [Ground roll and rest](#ground-roll-and-rest)
- [Surfaces](#surfaces)
- [Greens](#greens)
- [Cup and hole-out](#cup-and-hole-out)
- [Obstacles, canopy and walls](#obstacles-canopy-and-walls)
- [The ball actor in Unreal](#the-ball-actor-in-unreal)
- [Shot results](#shot-results)
- [Tests](#tests)
- [Calibration](#calibration)
- [Known gaps](#known-gaps)
- [Plugin structure](#plugin-structure)
- [References](#references)

## Overview

Every shot runs the same pipeline, in the headless simulator (`OpenFairway::ShotSimulator`) and in the ball
actor (`AOpenFairwayBall`), at 120 Hz:

```text
launch data ── spin parsing, launch vectors, air and landing response (FResolvedShotParams::Resolve)
     │
     ├─ in flight: RK4 step with gravity, drag, Magnus lift and spin decay ── contact? ──┐
     │                                                                                   ▼
     │                                    impact: restitution, friction, landing loss, bounce or settle
     │                                                                                   │
     └─ on the ground: rolling resistance, grass and air drag, slope, contact friction ◄─┘
                                      │
                               rest, or the cup (actor only)
```

- **The core** (`Public/OpenFairway/`, `Private/OpenFairway/`) is plain C++ on Unreal's `Core` module, in SI
  units, with no worlds or UObjects. It holds the equations, the surface catalog, the ball profile and the
  headless simulator.
- **The flight** is a fitted table of drag and lift against spin ratio, with a measured low-Reynolds drag rise,
  integrated with coupled RK4. Nothing about the club or the launch reaches it.
- **Contact and roll** were originally ported from the maintainer's
  [ogolf-physics](https://github.com/digitalhand/ogolf-physics) (MIT). The turf surfaces now read their landing
  loss from the impact itself and are fitted to launch-monitor roll-out.
- **The ball actor** finds contacts, normals and surfaces with Chaos sphere sweeps and line traces, and never
  simulates as a Chaos rigid body, so Chaos never competes with the equations for the ball's motion.
- **Shared steps** (`OpenFairway/ShotStep.h`): the impact and bounce test, settling, the flight to a contact
  and the landing angle are one implementation for both, so live and headless shots agree.

## Quick start

The plugin is enabled in `UOpenGolf.uproject`. To use it elsewhere, copy `Plugins/OpenFairwayPhysics/` into that
project's `Plugins/`, add `{"Name": "OpenFairwayPhysics", "Enabled": true}` to its `.uproject` and
`"OpenFairwayPhysics"` to the dependent module's `Build.cs`. It needs only the engine's `Core`, `CoreUObject`
and `Engine` modules.

### Blueprint

1. Place an **Open Fairway Ball** above ground that blocks the **Visibility** channel. It settles onto the
   ground at Begin Play.
2. Give the ground a material whose physical material is an **Open Fairway Physical Material** (the
   OpenGolfCourse plugin ships `PM_*`), or tag the ground (or its actor), such as `OpenFairway.Surface.Green`.
   Undeclared ground plays as the ball's **Default Surface**.
3. Make an **Open Fairway Launch**: ball speed (mph), vertical and horizontal launch (degrees), and the spin
   values you have, with their **Has** flags set.
4. Call **Launch**. Bind **On Ball Landed**, **On Ball At Rest** and **On Ball Holed**, or read
   **Get Shot Result** every frame.

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

`PlaceAt(Location)` stops the ball and sets it on the ground, facing the aim line. `AimYawDeg` turns launches
clockwise from the world +X target line. The **OpenFairway|Air** properties (`TemperatureF`, `AltitudeFt`,
`RelativeHumidity` 0–1, `PressurePa` with 0 deriving it from altitude, `WindSpeedMph`, `WindFromDeg`) and
`GreenStimpFeet` apply from the next launch. `GetWindVelocityMps()`, `GetWindVelocityAtHeightMps(HeightM)`,
`GetAirDensityKgM3()`, `GetWindReferenceHeightM()` and `GetWindRoughnessM()` report the air a moving shot flies
through (the settings at rest), so scenery can show it. `PredictShot` plays the launch headless on flat ground
of one surface with the ball's air and profile.

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
// Result.CarryM, TotalM, LateralM, ApexM, HangTimeS, LandingAngleDeg, LandingSpinRpm, bAtRest ...

OpenFairway::FAirConditions Air;  // 75 F, sea level, dry and calm by default
Air.RelativeHumidity = 0.5f;
Air.WindSpeedMph = 10.0f;
Air.WindFromDeg = 0.0f;  // headwind
const auto IntoWind = OpenFairway::ShotSimulator::Simulate(
    Launch, OpenFairway::ESurface::Fairway, OpenFairway::FBallPhysicsProfile(), Air);
```

`Simulate` also takes a time limit, a `GroundNormal` for a planar slope through the launch point, and
`FShotSimulationOptions` (step size, a per-step `Trace` callback, a surface override for calibration). Check
`bAtRest` before treating `TotalM` as final; false means the time limit (`DefaultMaxTimeSeconds`, 120 s) ran out.
The core functions (`GolfPhysics::StepFlight`, `ResolveImpact`, `StepGround`, `CanRest`) can be called directly
with an `FBallState` in SI units; build their `FFlightEnvironment` with `FResolvedShotParams::Resolve` or
`FAirConditions::ToEnvironment`.

## Units and frames

- **Launch input:** mph, degrees and rpm. Positive horizontal launch, spin axis and side spin are to the right.
- **Core:** SI units (m, m/s, rad/s) in the OpenFairway frame: +X downrange, +Y up, +Z right (right-handed).
  Unit factors are in `OpenFairway/Units.h`; spin is `GolfPhysics::RadPerRpm` (0.10472).
- **Unreal:** centimeters, +X forward, +Y right, +Z up. The actor converts positions and directions at its
  boundary; spin stays in the core frame, so cross products keep their physical meaning.
- **Wind:** `WindFromDeg` is the direction the wind blows from, clockwise seen from above from the +X target line:
  0 is a headwind, 90 comes from the right, 180 is a tailwind. It is fixed to the world and does not turn with
  `AimYawDeg`.
- **The ball:** mass 45.93 g, radius 21.335 mm, moment of inertia `0.4 m r²` (`GolfPhysics`).

## Launch

- **Spin** (`ShotSetup::ParseSpin`): back and side spin, or total spin and spin axis, whichever is given; the
  missing ones are derived. When measured back, side and total spin disagree by more than 1 rpm, the components
  win and total and axis are recomputed (launch monitors measure the components).
- **Launch vectors** (`ShotSetup::BuildLaunchVectors`): the velocity is pitched up by the vertical launch and
  turned by the horizontal launch; the spin, `(0, −side, back)` rad/s, turns with it. A putt with no measured
  spin leaves the face rolling (`GolfPhysics::LaunchSpin`), faded out by launch speed over 12–18 m/s so a topped
  full shot does not.
- **Into the ground:** a launch whose velocity points into or along the ground contacts it at t = 0. It has no
  carry and never flies (see [Topped shots](#topped-shots)).
- **Landing bucket** (`LandingBucket::Classify`), from the launch's backspin and vertical angle: **Run** below
  4000 rpm, **Release** below 5500, **Check** below 7500, and above that Check under 18° and **Spinback** from
  18°. Each bucket has a first-impact bite (`FBallPhysicsProfile::LandingResponses`):

  | Bucket | Bite scale | Largest spin-back, m/s |
  | --- | ---: | ---: |
  | Run | 0 | 0 |
  | Release | 0.50 | 0 |
  | Check | 0.85 | 0 |
  | Spinback | 1.00 | 1.20 |

  The response a shot gets is blended between neighbouring launch angles (5, 14, 21.5, 27.5, 31.5, 36.5°) and
  spins (1250, 3250, 4750, 6500, 8500 rpm), so there is no step at a bucket edge; the bucket itself is a label.
  The bite fades in over 8–20 m/s of impact speed and acts only on green and sand: turf has its own landing law.

## Air and wind

`FAirConditions` gives the air (defaults 75 °F, sea level, dry, calm):

- **Density:** the barometric formula from `AltitudeFt` at the air temperature, or a measured station pressure
  (`PressurePa`). Dry air is `p / (R T)`; humid air adds water vapour at its Tetens saturation pressure, the dry
  air keeping only its partial pressure, so humid air is lighter. 75 °F at sea level is 1.188 kg/m³.
- **Viscosity:** Sutherland's law.

Wind is slowed by the ground: it is about zero at the grass and grows with height by the neutral log profile,
`wind(h) = wind_ref · ln(h / z0) / ln(h_ref / z0)` (`WindProfile`):

| Setting | Default | Meaning |
| --- | --- | --- |
| `WindReferenceHeightM` | 10 m | Height of the authored `WindSpeedMph`, as in weather reports; 0 keeps the wind uniform |
| `WindRoughnessM` | 0.03 m | Roughness length of open short grass (WMO class "open", after Davenport and Wieringa) |

| Height | 1 m | 2 m (the flag) | 5 m | 10 m | 30 m |
| --- | ---: | ---: | ---: | ---: | ---: |
| Share of the authored wind | 60% | 72% | 88% | 100% | 119% |

- In flight the height is the ball centre's height above the launch ground (the core has no world access); a ball
  below its launch ground gets no wind. A ball on the ground meets the wind one radius above its own lie, wherever
  that lie is, which under the profile is none.
- The share is 0 at or below the roughness length and stops growing above 100 m. Settings that cannot describe a
  profile (non-finite, or a reference height inside the roughness) fall back to uniform wind.
- There are no gusts.

## Flight

Each 120 Hz step is one coupled RK4 step (`GolfPhysics::StepFlight`) of position, velocity and spin. At every
stage the wind is read at the stage's height and the aerodynamics are resampled:

- **Forces:** gravity (9.81 m/s²), drag `½ ρ A Cd v_air²` against the air-relative velocity, and Magnus lift
  `½ ρ A Cl v_air²` along `ω × v_air`. Below 0.5 m/s of air speed there are no aerodynamic forces, and lift needs
  more than 0.1 rad/s of spin.
- **Spin decay** (Smits and Smith): `dω/dt = −2×10⁻⁵ · v_air / R · ω`. A typical drive keeps most of its spin.
- **Coefficients** (`FlightAerodynamics::Sample`): functions of the spin ratio `W = |ω| R / v_air` and the
  Reynolds number `Re = ρ v_air 2R / μ` only, from the table in `Private/OpenFairway/SpinRatioData.h`:

  | W | 0.06 | 0.08 | 0.10 | 0.12 | 0.15 | 0.16 | 0.22 | 0.32 | 0.50 | 0.80 |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
  | Drag | 0.190 | 0.207 | 0.225 | 0.238 | 0.246 | 0.279 | 0.299 | 0.324 | 0.361 | 0.361 |
  | Lift | 0.130 | 0.145 | 0.179 | 0.198 | 0.225 | 0.239 | 0.285 | 0.332 | 0.353 | 0.413 |

  Both are piecewise linear in W and held at the ends; lift rises linearly from zero at no spin to the first knot.
  Below Re 100k, drag adds 0.6838 of the measured rise of a modern ball's drag over its Re 100k value (the drag
  crisis, `BallData.h`: 0.49 at Re 50k, 0.33 at 65k, 0.25 at 75k, 0.215 at 100k).
- **Coverage:** the table was fitted over Re 35k–215k and W 0.06–0.8 (about 36–175 mph ball speed). Outside it
  `FFlightAerodynamicsSample::bOutsideMeasuredCoverage` is set and the replay reports the time spent there.
- **Contact:** when a step ends below the ground, `ShotStep::FlyToContact` bisects the step (20 halvings) for the
  moment the ball meets the ground plane and flies to it, so a very low launch whose whole flight fits in one step
  still carries.

How the table was fitted, and how far each reference is from it, is in [Accuracy](Accuracy.md).

## Impact

An impact (`GolfPhysics::ResolveImpact`, through `ShotStep::Impact`) reads the incoming normal speed `vn` and the
**backspin across the ball's travel**: sidespin and spin about the contact normal cannot check a ball. In order:

1. **Restitution** `e`: a base curve `0.45 − 0.01 vn + 0.0002 vn²` (0.25 above 20 m/s), reduced by spin (up to
   30% by 1500 rpm, 70% by 3000 rpm), and that reduction scaled by impact speed (none of it at 0, half at 12 m/s,
   all from 25 m/s).
   An impact slower than 2 m/s (4 m/s for a later bounce) does not rebound; a later bounce keeps half its
   restitution. The surface scales it (sand 0.60) and sand has its own thresholds.
2. **Normal impulse** `m (1 + e) vn`.
3. **Friction impulse:** `−(2/7) m · slip` at the contact point, capped by kinetic friction times the normal
   impulse (the Coulomb limit). On a surface with a spin-back response above 1 (the green), the first impact's
   cap grows with the response once the landing is steeper than the critical angle, which a high-spin, fast
   landing raises (see [Greens](#greens)).
4. **Tangential loss.** On turf (any surface with `LandingAngleLoss` or `LandingSpinLoss`), the impact keeps
   `exp(−k · vn/vt) · max(0, 1 − min(c · R ωb, G) / vt)` of its tangential velocity and contact-plane spin, with
   `vt` the incoming tangential speed, `ωb` the backspin, k = 0.665, c = 0.464, and `G` the Coulomb bound per unit
   mass. A steeper landing digs in and keeps less; backspin grips, but only with load, so a ball skimming the turf
   keeps nearly all its pace. Elsewhere a first impact keeps `0.55 · clamp(1 − rpm/8000, 0.4, 1)`, faded in over
   0.75–2 m/s of normal speed, and a later bounce 0.70–0.85 by spin ratio. A grazing contact also spends less
   energy pushing into the surface (`ImpactRetentionScale`, sand 0.25).
5. **Sand plow:** each sand impact removes `SandPlowRatio` (2.5) times `vn` of forward speed, so a shallow
   full-shot landing stays near its pitch mark.
6. **Landing bite** (green and sand, first impact): Penner's `2Rω/7` check, times the bucket's bite scale and the
   surface's spin-back response, faded in by how far the landing is steeper than the critical angle and by impact
   speed (8–20 m/s). It removes forward speed down to zero, or down to the bucket's spin-back speed on a spin-back
   surface, and never adds energy.

After the impact the ball **bounces** if it leaves fast enough to rise 0.25/(2g) m (about 0.5 m/s on level
ground; sand 1 mm); otherwise it **settles** on the surface (its normal velocity removed) and rolls.

## Ground roll and rest

Each ground step (`GolfPhysics::StepGround`) solves resistance and contact together:

- **Grass drag:** linear `6π R η v` (η the surface's grass viscosity) integrated exactly over the step, and a
  speed-squared form drag `c_f v²` for blades the ball pushes through.
- **Air drag:** the ball on the ground still meets the air: a drag polynomial in Re times a spin-drag multiplier
  (`FlightAerodynamics::GroundDragCoefficient`, the model the turf and the green speed were calibrated with),
  scaled by `FBallPhysicsProfile::GroundDragScale` (1.01), against the wind one radius above the lie.
- **Rolling resistance:** `μr · N`. On green and turf it is the surface's own; on sand a landing's impact spin
  multiplies it (`SpinFrictionMultiplier`, up to 2.5 at 2750 rpm, less at low ground speed). The same multiplier
  scales the sliding friction of the contact on green and sand; turf takes neither.
- **Slope:** gravity's component along the ground drives the ball; the turf resists the drive up to
  `μr × 1.6` (`FRolloutProfile::SlopeResistanceRatio`) of the normal load, so a slope holds a resting ball up to
  `atan(1.6 μr)`.
- **Contact:** a Coulomb-capped impulse `−(2/7) m · slip` that turns sliding into rolling each step; spin decays
  in the grass and spin about the normal is resisted.
- **Stop and rest:** below 0.05 m/s with little contact-plane spin, on a slope the surface holds, the ball stops;
  `CanRest` then ends the shot. A ball that only pivots about the normal can rest.

### Topped shots

A full shot struck into or along the ground (it contacts the ground at t = 0) rolls for the rest of its shot with
its own share of the grass drag, on every surface it rolls over (`GolfPhysics::StruckAlongGround`): the turf's
linear grass drag times `StruckViscosityScale` (0.26) and its form drag times `StruckFormDragScale` (0.69). This is
the one rule in the roll that reads how the shot started. Launch-monitor exports roll topped shots about 1.2 yd per
mph, far farther than a ball that lands from a short, low flight in nearly the same state, and no rule that reads
only the ball's state fits both.

- The share fades in with launch speed over 12–18 m/s (27–40 mph), the same ramp that tells a putt from a topped
  shot, so putts and Texas wedges roll as before and stay continuous across 0°.
- At full speed, total distance steps at 0°: at 60 mph and 2000 rpm a ball struck along the ground goes much
  farther than one launched at 0.1°. The references step the same way. On each side of 0° the total changes by at
  most 15% per degree and 3% per 0.1° (`UOpenGolf.Physics.RolloutRegression`).
- Green and sand have no struck drag (scales 1).

## Surfaces

Declare a surface with an `UOpenFairwayPhysicalMaterial` (its **Surface** property) on the ground's material,
material instance or Landscape layer, so a mesh drawn with that material plays as that surface. Contact sweeps and
ground probes return the hit's physical material, so complex collision (Landscape layers, per-face materials)
resolves per contact. Engine physical-material friction does not affect the ball.

`AOpenFairwayBall::ResolveSurface` takes the first match:

1. An `OpenFairway.Surface.<Name>` tag on the hit component (`GetSurfaceTag(Surface)` returns it).
2. An `UOpenFairwayPhysicalMaterial`: the hit's, else the component's simple-collision material.
3. An `OpenFairway.Surface.<Name>` tag on the component's actor.
4. The ball's **Default Surface** (Fairway).

The catalog (`SurfaceCatalog`, `Private/OpenFairway/Surfaces.cpp`):

| Surface | Kinetic friction | Rolling friction | Grass drag η | Form drag, 1/m | Critical angle, rad | Stimpmeter |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Fairway | 0.50 | 0.0469 | 0.0242 | 0.0086 | 0.29 | 7.4 ft |
| Soft fairway | 0.56 | 0.0657 | 0.0342 | 0.0121 | 0.32 | |
| Rough | 0.62 | 0.0891 | 0.0456 | 0.0162 | 0.35 | 3.9 ft |
| Firm | 0.30 | 0.0281 | 0.0142 | 0.0051 | 0.25 | |
| Fringe | 0.56 | 0.0680 | 0.0349 | 0.0124 | 0.29 | |
| Green | 0.58 | from the stimp (0.0747 at 10 ft) | 0.0009 | 0 | 0.36 | the stimp |
| Sand | 0.65 | 0.45 (`SandRollingFriction`) | 0.012 | 0 | 0.45 | |

- **Turf** (fairway, soft fairway, rough, firm, fringe) shares the landing law (k 0.665, c 0.464) and the struck
  drag scales; impact spin does not multiply its friction. Only fairway was fitted (see [Calibration](#calibration));
  the others keep their ported ratios of rolling friction and grass drag to fairway's.
- **Sand** has restitution 0.60, grazing retention 0.25, its own bounce thresholds, the plow and a high rolling
  friction, so a ball that rolls in at 4 m/s stops within 2 m.
- The ball profile (`FBallPhysicsProfile`) can scale every surface's kinetic friction, rolling friction and grass
  drag, and offset its critical angle.

## Fairway speed

- **One setting for every fairway.** The ball's **Fairway Stimp Feet** (5–12, default 7.4;
  `FBallPhysicsProfile::FairwayStimpFeet` headless) sets the Fairway surface's rolling friction from the next launch. The
  game's **Settings › Game Defaults › Fairway speed** offers 6, 7.4, 8, 9 and 10 ft. Soft fairway, rough, firm, fringe,
  green and sand are unchanged, as is the fairway's grass drag.
- **The standard is the fit.** At 7.4 ft (`FairwaySpeed::StandardStimpFeet`, what the fitted fairway reads) the rolling
  friction is exactly the catalog's 0.0469. Any other reading scales it by the stimp solve's ratio,
  `RollingFriction(s) = 0.0469 * Solve(s) / Solve(7.4)`. The solve is the green's, run with the fairway's linear and
  speed-squared grass drag (both reach a rolling ball through 5/7, as the air drag does), so the solve's own error near
  the standard cancels.

  | Fairway stimp, ft | 6 | 7.4 | 8 | 9 | 10 |
  | --- | ---: | ---: | ---: | ---: | ---: |
  | Rolling friction | 0.0701 | 0.0469 | 0.0396 | 0.0298 | 0.0221 |

  A stimpmeter roll on level fairway measures each reading within 0.1 m (`UOpenGolf.Physics.FairwayStimp`). A faster
  fairway also holds a resting ball on less slope, since the holding slope scales with rolling friction.

## Greens

- **Speed.** The ball's **Green Stimp Feet** (8–15, default 10; the `uopengolf.GreenStimp` console variable on the
  range, `FBallPhysicsProfile::GreenStimpFeet` headless) sets every green's rolling friction from the next launch.
  `GreenSpeed::RollingFriction` solves, by bisection, for the constant deceleration that with the green's grass drag
  and the ground air drag in standard air stops a ball leaving a stimpmeter at 1.83 m/s at the stimp distance;
  rolling friction is `7 a0 / (5 g)`.

  | Stimp, ft | 8 | 10 | 11 | 12 | 13 | 15 |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | Rolling friction | 0.0943 | 0.0747 | 0.0675 | 0.0616 | 0.0566 | 0.0485 |
  | Holds a resting ball up to | 8.6° | 6.8° | 6.2° | 5.6° | 5.2° | 4.4° |

- **Grass drag** is 0.0009, below fairway's, so a ball rolling from fairway onto a green after a full-shot landing
  goes farther than on fairway, and a stimp-10 green out-rolls fairway after any landing spin. Fast putts meet
  less speed-dependent drag than on a high-drag green (see [Known gaps](#known-gaps)).
- **Landing.** The green has the strongest spin-back response (1.12, with up to 0.12 rad more critical angle for
  3500–5500 rpm landings at 8–20 m/s) and takes the landing bucket's bite, so a Check wedge holds and a Spinback
  flop draws back about 1.5 yd; the same shots on fairway do not spin back. `FShotSimulationResult` reports
  `FirstImpactTangentInMps`, `FirstImpactTangentOutMps` (negative after spin-back) and `bFirstImpactSpinback`.
- **Slopes and break.** The actor uses each contact's normal, so a contoured green breaks putts; `Simulate` takes a
  planar `GroundNormal`. Grain and firmness are not modelled.
- **Building greens.** A green is ground whose material carries `PM_Green` (`MI_Green`, `MI_TerrainSurface_Green` in
  OpenGolfCourse) or a component tagged `OpenFairway.Surface.Green`. The terrain builder sets `PM_<Surface>` on each
  region mesh; a green painted as a Landscape layer needs that layer's physical material set to `PM_Green`.

## Cup and hole-out

The green is a solid surface, so the cup is a capture rule (`OpenFairway/CupCapture.h`, after ogolf's
`CupCapture.cs`); a 108 mm hole in the ground mesh would meet the ball as a wall. Each cup has its own frame (its
centre at the surface, +Y its normal), so cups on slopes work the same way.

| Rule | Value |
| --- | --- |
| Cup | Regulation: 4.25 in (108 mm) across, 4 in (101.6 mm) deep |
| Rolling capture | Lip-entry model (Holmes 1991): a ball crossing along a chord drops if it falls one ball radius before the far rim, `MaxCaptureSpeed(offset) = 2 √(r_cup² − offset²) · √(g / 2 r_ball)`: about 1.64 m/s through the centre, 0 at the rim. A ball that stops over the cup is holed |
| Lip-out | A faster ball crosses the open cup (`AdvanceCrossing`): ballistic motion in the cup frame, sphere contact with the rim at impact time (restitution 0.1, friction 0.3, uncalibrated defaults). It hops over the far lip, turns across the cup, or hits the back and drops |
| Dunk | A descending contact inside the cup at green level, from flight or a bounce |
| Height gate | The centre within two ball radii (a contact within one) of the surface, so a ball on the flagstick never counts |
| Drop | The ball settles on its entry side, as close to the centre as the flagstick allows, fully below the surface |

On the actor, `SetCups(TArray<FOpenFairwayCup>)` registers cup positions (world cm) and normals; `SetCupLocations`
is the position-only form with world-up normals. A rolling ball's move each step is tested against every cup
before the green sweep (`CupCapture::ApertureEntry`); the earliest entry wins. A holed ball sinks over
`HoleDropSeconds` (0.35 s): `OnBallHoled` fires at capture and `OnBallAtRest` after the drop. The latest crossing's
entry speed, capture limit, offset, spin, exit velocity and `CupMaxHeightM` are in the shot result. The course kit
draws the cup and lifts the flag ([Open Golf Course](../../OpenGolfCourse/Docs/Reference.md#cup)).

## Obstacles, canopy and walls

The rules are in `OpenFairway/Obstacles.h`; the actor's statics of the same names forward to them.

- **Walls:** a contact steeper than about 45° (normal Y below 0.7) reflects the ball, which keeps 30% of its speed.
- **Obstacles:** components or actors tagged `OpenFairway.Obstacle` (`GetObstacleTag()`), such as tree-trunk
  colliders, always deflect: the ball reflects off the horizontal part of the contact at 30% speed and never lands
  or rests on them; one dropping onto a flat top leaves along its offset from the obstacle's axis at 1 m/s or more.
  Ground probes look through obstacles. OpenGolfSavanna tags its trunk collision.
- **Canopy:** components or actors tagged `OpenFairway.Canopy` (`GetCanopyTag()`) with an overlap-only response on
  the trace channel, such as a tree's foliage volume, never block. A ball in the air inside one keeps
  `exp(−path / 4 m)` of its speed and spin and turns by a seeded angle of at most `6° · √path`, seeded by instance
  and step so a repeated shot repeats.
- `PredictShot` and the headless simulator have no obstacles or canopy.

## The ball actor in Unreal

| Each 120 Hz step | How |
| --- | --- |
| Flight or ground step | `GolfPhysics::StepFlight` / `StepGround` |
| Move and find contacts | `SweepSingleByChannel` with a sphere 0.5 mm smaller than the ball (the true sphere when descending, so a grazing touchdown is not missed) |
| Stay grounded while rolling | `LineTraceSingleByChannel` ground probe |
| Impact, bounce, settle, roll, rest | The core, through `OpenFairway/ShotStep.h` as in the headless simulator |
| Surface under the ball | The hit's `UOpenFairwayPhysicalMaterial`, or `OpenFairway.Surface.*` tags |

- After a landing the ball spends the rest of the step moving with its rebound, with another sweep, as the headless
  loop does. A ball 0.5 m or more below the ground under it is lifted back; one 1000 m from its launch or 100 m below
  it stops as a safety stop, as does a shot still moving after `MaxShotSeconds` (120 s).
- Up to 12 steps run per frame; after a longer stall the excess time is dropped, so below about 10 fps simulation
  runs slower than wall time. The actor is shown between the last two steps, so it moves every frame at any frame
  rate (`GetSimulatedLocation()` is the step's position).
- **Presentation only:** the mesh turns as it rolls (`bVisualRoll`, at most a radian per frame so it never reads as
  turning backwards); flight spin is not shown. `bDrawTracer` draws a debug path; the game's shot trail is separate.
- Live and headless shots agree: `UOpenGolf.Range.FlightParity` and `RecordedContactParity` hold the live carry and
  total within 0.25 yd of `PredictShot`, at 30 and 144 fps and through frame stalls.
- The implementation is split by concern: `OpenFairwayBall.cpp` (lifecycle, launch, frame stepping, results, air),
  `OpenFairwayBallMotion.cpp` (the step and contact response), `OpenFairwayBallSurface.cpp` (surfaces, tags, ground
  queries), `OpenFairwayBallCup.cpp` and `OpenFairwayBallPresentation.cpp`.

## Shot results

`FOpenFairwayShotResult` is updated every frame while the ball moves and passed to the events:

| Field | Meaning |
| --- | --- |
| `CarryM` | Downrange distance at the first landing |
| `TotalM`, `RollM` | Current downrange distance (the total at rest), and `TotalM − CarryM` (negative after spin-back) |
| `LateralM` | Offline from the aim line, positive right |
| `ApexM` | Highest point above the launch point |
| `HangTimeS`, `LandingAngleDeg`, `LandingSpeedMps`, `LandingSurface` | First landing |
| `Surface` | Surface under the ball now (the lie at rest) |
| `LandingBucket` | Run, Release, Check or Spinback |
| `bLanded`, `bAtRest`, `bHoled` | Landed; at rest; dropped into a cup |
| `TerminationReason` | `Rest`, or `SafetyStop` past `MaxShotSeconds` or out of the playable area |
| `ElapsedS` | Simulated time since launch |

Distances are measured along the shot's horizontal launch direction.

## Tests

From the repository root (`bash Assets/Scripts/ue.sh test --filter UOpenGolf.Physics` on Linux):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Test -Filter UOpenGolf.Physics
```

| Test | What it checks |
| --- | --- |
| `UOpenGolf.Physics.GameplayPins` | 27 headless shots across every surface, air, wind and a sidehill putt, the stimp friction at 8, 10 and 13 ft and the ground drag: carry, total, offline (0.01 yd), apex (0.01 ft), hang time, landing angle and bounces pinned. A change here changed the physics |
| `UOpenGolf.Physics.Core` | Air properties, spin parsing, launch vectors, friction, rest and rebound rules, green speed |
| `UOpenGolf.Physics.FlightModel` | The flight table: values, never falling with spin, drag at least 0.17, the drag crisis and its continuity, lift against the measured modern ball, coverage, no dependence on the ground scales or heading; eight reference carries |
| `UOpenGolf.Physics.FlightIntegration` | Vacuum ballistics, spin decay keeping its axis, mirrored launches, no carry step with speed |
| `UOpenGolf.Physics.LandingResponse` | Landing buckets, the interpolated bite and its continuity across every bucket edge, backspin picking the bucket |
| `UOpenGolf.Physics.Wind`, `WindProfile`, `WindHeight` | Wind direction and frame, a ball moving with the air, the log profile and its limits, a flight step reading the ball's height, low and high shots in profiled and uniform wind |
| `UOpenGolf.Physics.RolloutRegression` | The ground step never adds energy and ends rolling; timestep convergence; spin about the normal; air drag on the ground; the turf landing law, its grip bound and its independence from the launch; topped shots and launch-angle continuity; turf speeds |
| `UOpenGolf.Physics.GreenPlay`, `GreenRollout`, `GreenStimp`, `SlopeRollout` | Putt distance and green speed, the landing buckets' bite, spin-back on the green and not the fairway, slopes; the green out-rolls fairway; the stimp reaches predictions; slope hold |
| `UOpenGolf.Physics.Sand` | Sand landings rest, the plow, rolling into a bunker |
| `UOpenGolf.Physics.CupCapture`, `CupContact` | The capture rules; lip crossings end, never gain energy and converge with smaller steps on flat and sloped cups |
| `UOpenGolf.Physics.ObstacleResponse`, `CanopyResponse`, `SurfaceResolution`, `VisualRoll` | Obstacle and canopy response and tagging, the surface resolution order, the visual roll |
| `UOpenGolf.Physics.ShotLibrary`, `ClubLabelIndependence` | Replays the shot library into `Saved/ShotLibrary/results.csv` (every shot rests, stays finite and converges within 0.1 yd of a 960 Hz run); club labels cannot change a shot |
| `UOpenGolf.Physics.ShotInterrupt` (game module) | A shot past its time limit ends as a safety stop; re-teeing mid-flight finishes the shot |
| `UOpenGolf.Range.Shot`, `FlightParity`, `RecordedContactParity`, `FrameRates`, `RecordedShots`, `RolloutSurfaces`, `ObstacleContact`, `CupThirtyFoot`, `HolePlay`, `GreenContour` (game module) | The ball actor in the range and on the hole: live against headless, frame-rate independence, surfaces, obstacles and canopy, cups and contours |
| `UOpenGolf.Calibration.Rollout` (opt-in) | Not a check: the turf fit (see [Calibration](#calibration)) |

## Calibration

The physics is checked and fitted against reference shots kept outside the repository
([Assets/Data](../../../Assets/Data/README.md#where-the-data-lives)); commands are in
[Development](../../../Docs/Development.md#physics-accuracy-scripts) and results in [Accuracy](Accuracy.md).

- **Flight:** `Assets/Scripts/flight_fit.py` reads the table and ball constants from the engine headers and flies
  the same RK4 flight in Python. `check` reports the shipping table; `fit` refits every knot; `refine` refits the
  low-spin knots toward one session while holding higher-spin launches. Both print a candidate table; nothing edits
  the engine.

### Turf calibration

`UOpenGolf.Calibration.Rollout` (`ue.ps1 Test -Filter UOpenGolf.Calibration.Rollout`, about two minutes) fits the
fairway to the local launch-monitor shots with a reference total, on flat fairway at 75 °F, grouping shots by launch
values only, and writes every candidate to `Saved/ShotLibrary/calibration.csv`. It never changes the catalog.

1. **Turf values:** k, c, rolling friction and both grass drags, scored on the roll error of the shots that fly.
   Every shot must rest; total may change by at most 14% per degree and 2.8% per 0.1° of launch (except across 0°);
   the fairway must be slower than the green and at least 6 ft on a stimpmeter; rough must hold a resting ball on a
   slope at least 1° steeper than a green does.
2. **Struck drag:** the two struck-along-the-ground scales, on the shots launched below 0°.

The catalog holds the result (`Private/OpenFairway/Surfaces.cpp`). The other turf surfaces follow fairway by their
ported ratios.

## Known gaps

- **Roll was fitted with earlier landings.** The flight lands about 1.3° steeper than the launch-monitor model the
  turf was fitted to, so mid launches roll about 2 yd short and one accuracy gate fails (see
  [Accuracy](Accuracy.md#roll)). `UOpenGolf.Calibration.Rollout` can refit the turf to the current landings.
- **The flight table is fitted, not measured:** only its low-Reynolds drag rise and its low-spin lift shape come from
  a measured ball. The references disagree by several yards for the same launch; outside about 36–175 mph the curves
  are held at their ends.
- **Headwinds** have not been compared with published wind data for this flight table.
- **Fast putts roll long.** With the green's low grass drag, a stimpmeter-speed putt rolls the stimp distance, but on a
  stimp-11 green a 9.5 mph putt rolls about 53 ft and a 12.5 mph putt about 84 ft, against the 40 and 60 ft putting
  targets in common use (`UOpenGolf.Physics.GreenPlay` reports them).
- **Topped shots:** matching them takes a rule that reads how the shot started, so total distance steps at 0° at full
  speed (see [Topped shots](#topped-shots)).
- **Still empirical:** the restitution curve and its spin cut, the later-bounce cutoff and scale, the green and sand
  landing (retention and the landing bucket's bite), the rim's restitution and friction, and the struck drag. No roll
  reference is faster than 121 mph ball speed, and none was hit on rough or the other turf surfaces.
- **Wind** has no gusts, and in flight its height is measured from the launch ground, not the terrain below the ball.
- **Not supported:** JSON ball profiles, surface zones and lie resolvers (use physical materials or tags), green grain
  and firmness.
- Packaged builds are only start-checked headless; ball flight in a rendered packaged game has not been reviewed.

## Plugin structure

```text
Source/OpenFairwayPhysics/
  Public/OpenFairway/            The core (Core module only)
    Units.h                        Unit factors
    Profiles.h                     Ball state, flight environment, flight, bounce and roll-out constants, surface settings
    Atmosphere.h                   Air density and viscosity
    FlightAerodynamics.h           Drag and lift (flight), and drag on the ground
    GolfPhysics.h                  Ball constants; flight, impact, ground and rest equations
    Surfaces.h                     The surface catalog and green speed
    BallProfile.h                  Ball profile, landing buckets, air conditions, per-shot parameters
    ShotStep.h                     Steps shared by the simulator and the actor
    ShotSimulator.h                Launch setup and the headless simulator
    WindProfile.h                  The log wind profile
    CupCapture.h                   Cup capture and lip-out
    Obstacles.h                    Obstacles, canopy and walls
  Private/OpenFairway/           Their implementation: Atmosphere, FlightAerodynamics, FlightStep, Impact, GroundRoll,
                                 Surfaces, BallProfile, ShotStep, ShotSimulator, WindProfile, CupCapture, Obstacles,
                                 the flight table (SpinRatioData.h), the measured drag (BallData.h) and CoreMath.h
  Public/OpenFairwayBall.h       The ball actor
  Public/OpenFairwayTypes.h      Blueprint types: surfaces, ball state, launch, cup, shot result
  Public/OpenFairwayPhysicalMaterial.h  Physical material that declares a surface
  Private/OpenFairwayBall*.cpp   The actor, split by concern (OpenFairwayBallInternal.h shares the frame conversion)
  Private/Tests/                 Automation tests; PhysicsTestUtils.h and ShotLibrary.h are their shared helpers
```

## References

- [ogolf-physics](https://github.com/digitalhand/ogolf-physics) (MIT, the maintainer's own project): the original
  impact, ground, rest, surface and cup-capture equations.
- A. R. Penner, [The physics of golf](https://doi.org/10.1088/0034-4885/66/2/202), *Reports on Progress in Physics*
  66 (2003), 131–171: flight, impact and rolling models, and the `2Rω/7` check.
- A. M. Nathan, [The Effect of Spin-Down on the Flight of a Baseball](https://baseball.physics.illinois.edu/spindown.pdf)
  (2008), section II, equation 5 and footnote 5: Smits and Smith's golf-ball spin decay and its SI coefficient `2e-5`,
  measured over Re 100k–250k.
- A. J. Smits and D. R. Smith, "A new aerodynamic model of a golf ball in flight," in *Science and Golf II* (1994),
  340–347, cited through Nathan: the spin-decay law and spin-dependent drag and lift.
- J. Lyu, J. Kensrud, L. Smith and others, "Aerodynamics of Golf Balls in Still Air," *Proceedings* 2(6), 238 (2018),
  [doi:10.3390/proceedings2060238](https://doi.org/10.3390/proceedings2060238), CC BY 4.0: drag and lift of a modern
  ball; the low-spin lift the flight fit started from.
- J. Lyu, J. Kensrud and L. Smith, "The reverse Magnus effect in golf balls," *Sports Engineering* 23, 7 (2020),
  [doi:10.1007/s12283-020-0318-1](https://doi.org/10.1007/s12283-020-0318-1), CC BY 4.0: drag of a modern ball against
  Reynolds number, the drag crisis in `BallData.h`.
- P. W. Bearman and J. K. Harvey, [Golf Ball Aerodynamics](https://doi.org/10.1017/S0001925900007617), *Aeronautical
  Quarterly* 27 (1976), 112–122: wind-tunnel lift and drag (background).
- Holmes (1991), as reviewed by Penner (above): the lip-entry capture speed of a rolling ball at the cup.
- World Meteorological Organization,
  [Guide to Meteorological Instruments and Methods of Observation (WMO-No. 8)](https://www.weather.gov/media/epz/mesonet/CWOP-WMO8.pdf),
  seventh edition (2008), Part I, Chapter 5: the 10 m standard wind height, and Davenport's terrain classes as adapted
  by Wieringa, whose "open" class has z0 = 0.03 m.
- NASA Glenn, [Viscosity](https://www.grc.nasa.gov/www/BGH/viscosity.html): Sutherland's law.

## License

MIT, as part of uopengolf. The equations ported from ogolf-physics come from the maintainer's own MIT project; see
[THIRD_PARTY_NOTICES.md](../../../THIRD_PARTY_NOTICES.md).
