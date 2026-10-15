# Optional range visuals

The public range uses distributable assets and starts normally without optional
plugins. A local content plugin can supply a different range subclass and a map
whose World Settings selects its game mode. All gameplay remains in C++.

## Table of contents

- [Using a local variant](#using-a-local-variant)
- [Authoring boundary](#authoring-boundary)
- [Verification](#verification)

## Using a local variant

Install optional packs under the ignored `Plugins/PrivateContent/` directory.
Follow the installed plugin's README for its map path and acquisition requirements.
In the Content Browser enable **Show Plugin Content**, open the variant map, and
press Play. Open the public `Content/UOpenGolf/Maps/UOpenGolf` map to return to the
baseline. The public startup map does not change.

The same replay controls, launch-monitor input, HUD and grass toggle apply to both
maps. `uopengolf.Grass3D 0` clears every decorative grass variant; `1` recreates the
deterministic population. Ground materials remain visible with grass disabled.

## Authoring boundary

`AUOpenGolfGameMode.RangeClass` defaults to the native range. A private Blueprint
game-mode subclass selects a private Blueprint range subclass. Range defaults
expose mesh variants, ground material, spacing, scale, radius and cull distances.
An empty mesh list retains the public clump. Mesh variants share the population
rather than multiplying its density; they are applied when Play begins.

The optional map and Blueprint defaults own private asset references. Public code,
maps and configuration must contain no required references to those assets.
Decorative grass has no collision, overlap events or navigation influence. Material
overrides apply to the existing Fairway-tagged ground without changing collision
or contact surface tags. Presentation settings are independent of ball physics.

Use Unreal asset moves to relocate Fab imports and update references. Retain a
local backup and audit dependencies before removing residual redirectors. Demo
characters and legacy landscape examples need not be part of a range variant.
Record the acquired license, provenance and modifications in the private plugin;
do not redistribute its editable assets as MIT.

## Verification

Build the editor, then run Smoke, `UOpenGolf.Range` and `UOpenGolf.Physics` using the
normal project scripts. For an installed variant, set `UE_ROOT` and run:

```powershell
python Scripts/check_range_variant.py --map /YourPlugin/Maps/YourRange
python Scripts/check_range_variant.py --map /YourPlugin/Maps/YourRange --render
python Scripts/check_range_variant.py --map /Game/UOpenGolf/Maps/UOpenGolf --render
```

The first command runs both maps, verifies a different range subclass, mesh
resolution, collision-free grass, deterministic toggle behavior, and matching iron
and low-wood carry, total and resting position. The map argument is required; this
optional test reports that it was not requested when run without that argument.

Rendered runs capture matching tee views with grass on/off, the ball, flag, grass
closeup and coverage edge at 1280x720. Inspect the PNGs under
`Saved/Screenshots/RangeAssets/`, including grass scale, wind, blending, ball
visibility and culling. Reports under `Saved/Automation/` include mean editor frame
intervals after settling. Compare idle runs on the same machine; these are relative
editor measurements, not packaged-game benchmarks. Cold shader compilation may
require a larger `--timeout` than the default 600 seconds.

Finally, temporarily remove the private plugin and build/run Smoke to verify a
fresh public checkout needs no restricted assets. Record actual platform evidence
in [phase status](PhasesStatus.md). Packaging is a separate milestone.
