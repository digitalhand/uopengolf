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

To use a variant's range on the default map too, create the ignored
`Config/UserGame.ini` on that machine:

```ini
[/Script/UOpenGolf.UOpenGolfGameMode]
LocalRangeClass=/YourPlugin/Blueprints/BP_YourRange.BP_YourRange_C
```

The game mode uses it only when the map keeps the native range. Automation tests
ignore it, so they always check the public range. If the class does not load (for
example, the plugin was removed), the log shows a warning and the public range is used.
Delete the file to go back to the public look.

The same replay controls, launch-monitor input, HUD and grass toggle apply to both
maps. `uopengolf.Grass3D 0` clears every decorative grass variant; `1` recreates the
deterministic population. Ground materials remain visible with grass disabled.

### A private copy of the range map

A private plugin can also hold its own copy of the range map with a copied terrain
definition, adding its own vegetation areas (for example trees) and rebuilding them with
`Scripts/RebuildRangeVegetation.py --map <private map> --generated <private folder>`.
Gameplay, trunk colliders and surfaces still come from the public C++ and kit. To open
that map by default on one machine, set it in the ignored `Config/UserEngine.ini`:

```ini
[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/YourPlugin/Maps/YourRange
GameDefaultMap=/YourPlugin/Maps/YourRange
```

`User*.ini` files are not packaged, and public maps and tests are unaffected. Remove the
section to return to the public range.

## Authoring boundary

`AUOpenGolfGameMode.RangeClass` defaults to the native range. A private Blueprint
game-mode subclass selects a private Blueprint range subclass. Range defaults
expose mesh variants, ground material, spacing, scale, radius and cull distances.
An empty mesh list retains the public clump. Mesh variants share the population
rather than multiplying its density; they are applied when Play begins.

The optional map and Blueprint defaults own private asset references. Public code,
maps and configuration must contain no required references to those assets.
Decorative grass has no collision, overlap events or navigation influence and is
clipped to the 34 m fairway strip. `GroundMaterialOverride` applies to the fairway,
fringe and green (`AUOpenGolfRange::TurfTag`). `RoughMaterialOverride` applies to the
rough strips and the collision-free backdrop (`RoughTag`). `ExposureBiasEV` (default
-0.5) sets the range's fixed exposure. Overrides change only the look: at Begin Play the
range pins each piece's physical material from its course-kit surface material
(`MI_Fairway` plays as Fairway, `MI_Rough` as Rough), so collision and contact surfaces do
not change. Presentation settings are independent of ball physics.

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
resolution, collision-free grass, turf kept off the rough, deterministic toggle behavior, and matching iron
and low-wood carry, total and resting position. The map argument is required; this
optional test reports that it was not requested when run without that argument.

Rendered runs capture matching tee views with grass on/off, the ball, flag, grass
closeup and coverage edge. They request 1280x720; embedded PIE can use the saved
editor viewport size instead, so compare the actual PNG dimensions. Inspect the PNGs under
`Saved/Screenshots/RangeAssets/`, including grass scale, wind, blending, ball
visibility and culling. Reports under `Saved/Automation/` include mean editor frame
intervals after settling. Compare idle runs on the same machine; these are relative
editor measurements, not packaged-game benchmarks. Cold shader compilation may
require a larger `--timeout` than the default 600 seconds.
Captures wait for shader compilation and include stationary sequences at the tee,
closeup and culling edge. Add `--render --shot-trail` to capture driver, iron and
slow-shot tracer behavior. The calm range disables grass displacement, and the
private material blends blade color with its underlying turf to reduce edge shimmer.
Use `--render --static-only` for a shorter six-view timing/capture run without
the repeated stationary screenshots.

For moving-shot CPU timing without screenshots, add `--render --shot-trail
--trail-timing`. This mode explicitly sets and checks a 1280x720 render target,
disables VSync, caps at 120 FPS, and waits ten seconds after shaders finish. Repeat
with `--legacy-trail` to compare the previous tracer geometry path in a development
automation build. Run comparisons sequentially with the same map and settings;
compare tracer CPU time separately from whole-frame intervals. See
[range asset validation](RangeAssets.md#validation) for complete commands and
[phase status](PhasesStatus.md#reference-measurements)
for measured results and limitations.

Finally, temporarily remove the private plugin and build/run Smoke to verify a
fresh public checkout needs no restricted assets. Record actual platform evidence
in [phase status](PhasesStatus.md). Packaging is a separate milestone.
