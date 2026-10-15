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
- [Shot results](#shot-results)
- [Units and coordinate frames](#units-and-coordinate-frames)
- [Tests and golden fixtures](#tests-and-golden-fixtures)
  - [Regenerating fixtures](#regenerating-fixtures)
- [Differences from ogolf-physics](#differences-from-ogolf-physics)
- [Known gaps](#known-gaps)
- [Plugin structure](#plugin-structure)
- [License](#license)

## Status

| Area | State |
| --- | --- |
| Core equations (flight, impact, ground, rest) | Ported; matches the C# reference on 21 fixtures to 0.0001 yd |
| Headless simulator (`ShotSimulator`) | Ported from `PhysicsAdapter`; flat ground, one surface |
| Ball actor (`AOpenFairwayBall`) | Working on Windows; automated range shot within tolerance of the headless result |
| Surfaces | All seven presets; assigned by tags |
| JSON ball profiles, surface zones, terrain probes | Not ported yet (see [Known gaps](#known-gaps)) |
| Linux | Code is portable; not built or run on Linux yet |

Ported from ogolf-physics revision `e6d793ae9477efaf55dc9078d7260f76bb532a7d`.

## Features

- Aerodynamic drag and Magnus lift from wind-tunnel polynomial fits, with the calibrated
  spin-drag, low-launch, high-launch and mid-spin corrections.
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
clockwise from the world +X target line.

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
// Result.CarryM, Result.TotalM, Result.LateralM, Result.ApexM, Result.HangTimeS, Result.bAtRest
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
in ogolf-physics. Steps run at 120 Hz with up to 12 substeps per frame.

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
landing responses and the launch-regime overrides. Each launch is classified as
`<family>-<speed>-<launch>-<spin>` (for example `D-S3-V0-P0`), and the most specific
matching override scales drag, lift and friction. The shot log names the regime and
the matched override. Changes to the profile apply from the next launch.

Air conditions come from the ball's `TemperatureF` (default 75 °F) and `AltitudeFt`
(default 0), the same defaults as the ogolf-physics headless simulator.

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
- **Unreal:** centimeters with +X forward, +Y right, +Z up (left-handed). The ball actor
  converts positions and directions at its boundary; spin stays in the core frame, so
  cross products keep their physical meaning.

## Tests and golden fixtures

Run from the repository root (Windows shown):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Test -Filter UOpenGolf.Physics
```

| Test | What it checks |
| --- | --- |
| `UOpenGolf.Physics.Core` | Air properties, spin parsing, launch vectors, regime keys, landing buckets, friction, rest and rebound rules, green speed |
| `UOpenGolf.Physics.Fixtures` | 21 headless shots against C# golden values: carry, total, offline (0.1 yd), apex (0.1 ft), hang time (0.01 s) and landing bucket, across all seven surfaces, a putt and a spin-back |
| `UOpenGolf.Range.Shot` (game module) | A drive hit in the running range lands and stops within 1 yd carry / 2 yd total of the headless prediction |

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

## Differences from ogolf-physics

- No exceptions: an invalid contact normal falls back to up, and launches are validated
  (`FLaunchData::IsValid`) before simulating.
- Single-precision math, as in C#. The port uses `MathF.PI`'s value, not the engine's
  double-precision pi, to stay bit-close.
- Out of bounds is 1000 m from the launch point (ogolf-physics uses the world origin), and
  a ball 100 m below its launch point stops instead of using Godot's terrain recovery.
- The ball actor ends a substep at the contact point; the headless loop spends the rest of
  the step after a bounce. The automated range test bounds the difference.

## Known gaps

- JSON ball profiles (`BallPhysicsProfile.FromJson`) are not ported; change `Profile` in C++.
- `SurfaceZone` area overrides, Terrain3D height queries and the lie resolver are not ported;
  use surface tags.
- No wind; no shot camera, tracer styling or HUD here (the game module supplies those).
- Physical-material surfaces are a possible later addition to tags.
- Linux builds have not been run.

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

## License

MIT, as part of uopengolf. The equations and calibration come from ogolf-physics
(`addons/openfairway`, MIT), the maintainer's own project; see
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).
