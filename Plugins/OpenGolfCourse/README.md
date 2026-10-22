# Open Golf Course

Reusable course kit for uopengolf maps: surface materials that declare OpenFairway physics
surfaces, a putting green, a flagstick whose cloth flag flies in the wind, a marked golf ball
and turf grass. The range uses it, and new
course designs can use the same pieces without range code.

The plugin depends on [OpenFairway Physics](../OpenFairwayPhysics/README.md) for the surface
type. Its content mounts at `/OpenGolfCourse/`.

## Table of contents

- [Contents](#contents)
- [Using the kit](#using-the-kit)
- [Performance](#performance)
- [Editing and regenerating assets](#editing-and-regenerating-assets)
- [Sources and licenses](#sources-and-licenses)
- [Tests](#tests)
- [Plugin structure](#plugin-structure)
- [Course grass](#course-grass)

## Contents

| Asset | Purpose |
| --- | --- |
| `Surfaces/PM_<Surface>` | One `UOpenFairwayPhysicalMaterial` per surface: Fairway, FairwaySoft, Rough, Firm, Green, Sand, Fringe |
| `Surfaces/M_Turf` | Parent turf material: world-aligned grass textures, mowing bands, a two-color palette |
| `Surfaces/MI_Fairway`, `MI_Rough`, `MI_Green` | Turf instances; each points at its physical material, so assigning one sets look and physics together |
| `Surfaces/SM_GreenDisc` | Putting green 20 ft in radius with `MI_Green`; its top sits 1 mm above the ground and its collision bevels down to it at the edge |
| `Surfaces/MF_MowStripes` | Mowing-band mask for any direction and band width, filtered against moire |
| `Surfaces/T_Grass_*` | ambientCG Grass 004 color, normal and roughness maps |
| `Grass/SM_GrassClump`, `M_GrassBlades` | 16-blade clump for instanced scatter; blades share the turf's bands |
| `Flagstick/SM_Flagstick`, `M_Pole` | Seven-foot, half-inch pole |
| `Flagstick/M_FlagCloth`, `MI_Flag_Red` | Two-sided flag cloth material (`FlagColor`, `FlagRoughness`) and its red instance |
| `Ball/SM_GolfBall`, `M_GolfBall`, `MF_BallMarking` | Dimpled ball (three LODs) with an alignment line and a logo dot |
| `UOpenGolfFlagstickComponent` (C++) | The pole plus a simulated 20 x 14 inch cloth flag (`FOpenGolfFlagCloth`); no collision; `SetWind` flies it |

## Using the kit

### Surfaces

Assign `MI_Fairway` or `MI_Rough` to a mesh (or use them as Landscape layer materials), and
the ball plays that surface. No component tags are needed: the material's physical material
declares it. For another surface, make a `M_Turf` instance, set its colors, textures and
bands, and set its **Phys Material** to the matching `PM_*`. An
`OpenFairway.Surface.<Name>` component tag still overrides the material; see the physics
README's [surfaces](../OpenFairwayPhysics/README.md#surfaces) for the resolution order.

`M_Turf` parameters:

| Group | Parameter | Default | Notes |
| --- | --- | --- | --- |
| Grass appearance | `GrassDarkColor`, `GrassLightColor` | sRGB #406B1F, #4D7E20 | Palette endpoints, stored as linear RGB |
| Grass appearance | `DetailStrength` | 0.12 | How much texture brightness moves the color between the endpoints (rough: 0.6) |
| Mowing | `StripeAngleDeg` | 0 | Mow direction from world +X; set it to the hole's heading |
| Mowing | `StripeOriginCm` | (0, 0) | A point on the center of a light band, such as the tee |
| Mowing | `StripeWidthCm` | 377.8 | One band; nine bands span the range's 34 m fairway |
| Mowing | `StripeContrast` | 0.5 | 0 hides the bands (rough), 1 is full dark to light |
| Texture | `TurfColor`, `TurfNormal`, `TurfRoughness`, `TextureSizeCm` | Grass 004, 140 cm | Swap for sand or other surfaces |

`MI_Green` is a lighter, finer bentgrass: sRGB #4C8228 to #5A9332, a 70 cm texture tile and
0.08 detail, with no mowing stripes (`StripeContrast` 0). Place `SM_GreenDisc` with its origin on
the ground at the hole, over turf whose top is at that height. The disc casts no useful shadow
onto turf 1 mm below it; turn its shadow off.

Bands are procedural, so they get no mip filtering. Their edges widen with the pixel
footprint, and their contrast fades to the mean as bands narrow toward the horizon, which
keeps them from aliasing into moire under temporal anti-aliasing.

### Flagstick

Add an **Open Golf Flagstick** component to any actor, with its origin on the ground at the
hole. The component draws the pole. Its `FlagCloth` child, a procedural mesh, draws the flag,
which `FOpenGolfFlagCloth` simulates every tick. Recolor it by setting `FlagMaterial` to another
`M_FlagCloth` instance, as `MI_Flag_Red` is.

Call `SetWind(WindVelocityMps, AirDensityKgM3)` whenever the wind changes. The range calls it
with the ball's wind (`AOpenFairwayBall::GetWindVelocityMps`).

The public range exposes wind on **Range settings → Details → UOpenGolf → Wind**. Set
**Wind Speed (mph)** to 10 and **Wind From (degrees)** to 90. With **Preview Wind in Editor**
and viewport Realtime enabled, the flag should extend downwind and flap after 5–10 seconds
before Play. The wind settings save with the map and also drive the ball during Play.
Use 20 mph for a stronger wind, 270 degrees to reverse it, or zero speed for calm air.
Console overrides remain available during Play as `uopengolf.WindMph` and
`uopengolf.WindFromDeg`. These controls belong to the game module; a standalone kit
component needs its caller to supply `SetWind` and enable editor ticking for a preview.
`WindDirectionalSource` is not connected automatically.
The flag follows the active shot's captured wind until that shot ends. Full steps and
the ball's editable wind fields are in
[range wind validation](../../Docs/RangeAssets.md#validate-wind-and-flag-movement).

The cloth:

- **Grid.** A 17 x 9 particle grid (3.2 x 4.4 cm cells) of 0.11 kg/m^2 nylon (`ClothKgPerM2`).
  The hoist column is pinned to the pole.
- **Solver.** XPBD with fixed 1/480 s substeps and four constraint passes each, so the result
  doesn't depend on frame rate.
  - Along and down the flag the cloth doesn't stretch (long-range attachments to the hoist hold it
    under 3%).
  - It shears and bends freely, which is what lets a flag pinned along its hoist fall.
  - Particles stay outside the pole.
- **Air.** Gravity pulls each particle. The air acts on each of the 256 triangles with
  flat-plate pressure across the cloth (`NormalForceCoefficient`, 1.2) and skin friction along it
  (`FrictionCoefficient`, 0.015), from the air's speed relative to the cloth.
  - A flag in its own plane feels only friction, so the wind holds it out through the drag of its
    own flapping, as on a real flag.
  - Gusts of about 15% in speed and 8 degrees in direction drift down the flag.
- **Sleeve.** The hoist sleeve turns downwind on the round pole over about a second, and stays
  put in still air.
- **Start.** The flag settles in calm air for two seconds when it is created, so it never
  starts sticking out.

Measured on the solver (the `UOpenGolf.Course.FlagCloth` test checks the same behavior):

| Wind | Middle of the flag | Free edge |
| --- | --- | --- |
| Still air | Hangs about 80 degrees below horizontal, at rest | Top corner about 47 cm below the pole top |
| 5 mph | Lifts to about 25 degrees | Flaps |
| 10 mph | Flies out, about 11 degrees | Flaps about 3.5 cm |
| 20 mph | Nearly straight, about 4 degrees | Flaps |

### Ball

Use `SM_GolfBall` on a ball actor; the ball actor normalizes its size. `M_GolfBall` exposes
`BallColor`, `LineColor`, `DotColor`, `LineHalfWidth` and `DotRadius`. The line is the
mesh's local XZ great circle, with the dot on it at local -X. `AOpenFairwayBall` turns the
mesh as it rolls and faces it down the aim line on each re-tee, so the line points at the
target and the dot tumbles as the ball rolls.

## Performance

- The flag is 153 particles and 256 triangles. Each frame runs about eight 1/480 s substeps
  of roughly 1,000 distance constraints, plus a 153-vertex mesh-section update: tens of
  microseconds. Creating it settles two seconds of calm air once.
- `MI_Fairway`, `MI_Rough` and `MI_Green` change only scalar and vector parameters, so they share
  `M_Turf`'s shaders and add no permutations.
- The ball marking is a few pixel-shader instructions on a small object; the visual roll
  is one quaternion multiply per frame while rolling.
- Physical-material lookup adds `bReturnPhysicalMaterial` to the ball's existing contact
  queries.

## Editing and regenerating assets

For the green-stripe and duplicate-flag repair, use
`Scripts/FixGreenAndFlag.py` from this plugin after exporting the pole-only source.
It changes the green's mowing contrast, pole mesh and flag material. It does not rebuild
rough, fairway, grass, ball or source textures. See
[targeted repair](../../Docs/RangeAssets.md#targeted-green-and-flag-repair).
The full generator below reapplies defaults throughout the kit; use it only when that
broader rebuild is intended.

**Editable Blender sources** are kept outside the repository, in `OpenGolfCourse/` under the
Blender art root:
`SM_GolfBall.blend`, `SM_Flagstick.blend`, `SM_GrassClump.blend`, `SM_GreenDisc.blend`,
`SM_GreenSurround.blend`, and `Coastal/` for the coastal grass.
- The green's collision hull is `UCX_SM_GreenDisc_00`, and the surround has separate hulls
  around its open center.
- The art root is `--art-root PATH`, else `$UOPENGOLF_BLENDER_ROOT`, else
  `~/Code/3D/blender/uopengolf`; see `Scripts/art_root.py`.
- The repository holds only the imported Unreal assets, so the first export creates any
  missing `.blend` from the pinned CC0 sources and the generators.

Edit one in Blender, then export and rebuild:

```bash
# 1. Export FBX from the .blend files (Saved/AssetImport/Prepared). The first run downloads
#    and checksums the CC0 sources and creates any missing .blend; --regenerate rebuilds the
#    .blend files from the sources and generators, discarding edits.
blender --background --python Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py
blender --background --python Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py -- --regenerate
blender --background --python Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py -- --art-root /path/to/art

# 2. Build or update every /OpenGolfCourse asset. In the editor's Output Log:
#      py "<repo>/Plugins/OpenGolfCourse/Scripts/BuildCourseAssets.py"
#    or headless, with the editor closed:
"$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PWD/UOpenGolf.uproject" \
  "-ExecutePythonScript=$PWD/Plugins/OpenGolfCourse/Scripts/BuildCourseAssets.py" \
  -unattended -nopause -nosplash -nullrhi
```

On Windows, use `blender.exe` and `UnrealEditor-Cmd.exe` the same way. The flagstick mesh is
the pole alone, in one material slot.

`BuildCourseAssets.py` is idempotent. It finds every graph node by its description and
creates only what is missing, then reapplies settings and links. It never deletes nodes,
because materials loaded by C++ constructors keep their expressions rooted. It deletes an
obsolete generated asset only once nothing references it, and otherwise warns. The flag
builder retains the old `M_Flag` and `MF_FlagWave` assets unused; the cloth uses `M_FlagCloth`.
Do not delete graph nodes from materials loaded by C++ constructors. The builder refuses to
replace assets without its `Generator` metadata tag. It reloads each new material function
once so the engine assigns its input and output Ids; without them, every pin on a function
call collapses onto the first output. Use the editor or `-ExecutePythonScript`, not the
Python commandlet, because LOD generation needs editor subsystems.

## Sources and licenses

| Input | Conversion |
| --- | --- |
| ambientCG `Grass004_2K-JPG.zip` (CC0) | Color as sRGB, NormalDX as a normal map without flipping green, Roughness as linear masks. World-space UVs repeat every 140 cm; world-space normals match the flat ground. |
| jpsweber `golf_0.zip`, `Golf_ball.obj` (CC0) | Centered and normalized to a 100 cm diameter, smooth normals. LODs at 100%, 25% and 5% of 61,672 source triangles, screen sizes 1.0, 0.1 and 0.025. |
| Original flagstick (MIT) | 32-sided half-inch pole, seven feet above ground plus 10 cm buried. The flag is generated cloth. |
| Original putting green (MIT) | 128-segment disc, 20 ft in radius, 1 mm high, with a single convex `UCX_` collision hull beveled to the ground over its outer 30 cm. |
| Original grass clump (MIT) | 16 deterministic blades over a 20 cm square, 2-3 cm tall, 0.8-1.2 cm wide at the base. |

Archive SHA-256 checksums pin the downloads in `Scripts/PrepareCourseAssets.py`. See the
repository's [third-party notices](../../THIRD_PARTY_NOTICES.md). Material graphs are
original MIT contributions.

## Tests

`UOpenGolf.Course.Assets` checks the following:

- A physical material exists for every surface.
- `MI_Fairway`, `MI_Rough` and `MI_Green` play as Fairway, Rough and Green and share `M_Turf`;
  the green has no stripes.
- `SM_GreenDisc` is 20 ft in radius, 1 mm high, with one convex hull.
- The flagstick mesh is the pole alone, with the right height and no collision; its flag material is
  two-sided, with no world-position offset.
- The ball material exposes its marking parameters.

`UOpenGolf.Course.FlagCloth` runs the cloth headless:

- Still air: it hangs and comes to rest.
- 10 mph: it flies downwind and flaps.
- 5 mph sags more than 10 mph, and 20 mph flies higher.
- Nothing stretches more than a few percent.
- 30 and 144 fps agree.

The game's `UOpenGolf.Range.*` tests cover the range built from the kit.

## Plugin structure

```text
OpenGolfCourse.uplugin
Content/                  /OpenGolfCourse assets (Surfaces, Grass, Flagstick, Ball)
Scripts/
  PrepareCourseAssets.py  Download/verify sources; Blender: .blend -> FBX
  art_root.py             Where the editable .blend projects live (outside the repo)
  BuildCourseAssets.py    Editor entry point: builds every asset
  course_assets.py        Idempotent asset and graph helpers
  course_surfaces.py      Physical materials, mowing bands, turf, grass blades
  course_props.py         Flagstick and ball
Source/OpenGolfCourse/
  Public/OpenGolfCourse.h               Asset paths
  Public/OpenGolfFlagstickComponent.h   Flagstick component (pole + cloth flag)
  Public/OpenGolfFlagCloth.h            Flag cloth solver
  Private/Tests/                        Asset tests
```

## Course grass

A terrain area can carry a **course grass layer**: dense grass right up to the play edges,
placed and baked on its own so trees and other vegetation stay untouched. Set it on the
`AOpenGolfTerrainArea` (Details → Terrain → Course grass):

- `CourseGrassPresets`: ground-cover presets, for example dense tiles near play and sparse
  clumps further out.
- `BoundaryGrassPreset`: edge strips along every play outline.

**Tiled placement** (`PlacementPattern = Tiled`) is for seamless square grass tiles. The preset
lists `Tiles`, largest first, each half the one before (for example 1, 0.5 and 0.25 m), each
with meshes that fill exactly that square. The grid follows the terrain's axes and tiles turn
only in quarter turns, so neighbours meet edge to edge. A cell whose whole footprint is on the
preset's surface, `BoundaryClearanceM` from every region outline and the level tee, gets one
tile. Cells crossing an outline, or on ground whose normal turns more than
`MaximumTileTiltDeg` across them, are halved, down to the smallest tile, which needs only its
centre to pass. The distance band (`Minimum/MaximumDistanceFromPlayM`) is judged at tile
centres, so its outer edge stays soft. Tiles sink `TileSinkM` into the ground so their edges
never float.

**Boundary strips** run along the rough side of every fairway, fringe, green, tee and sand
outline. Each strip (`EdgeStripSizeM`, X along the edge, Y into the rough) is turned to follow
the edge, its inner edge sits at `BoundaryClearanceM`, and neighbours overlap by 15% so bends
leave no gap. Strips that would come within clearance of another play surface are skipped.

Apply a layer with `Scripts/ApplyCourseGrass.py` in the editor, with the course map open and
Play stopped:

```text
--map /YourCourse/Maps/Course --generated /YourCourse/Generated
--grass-preset /YourArt/Terrain/DenseGrass
--boundary-preset /YourArt/Terrain/BoundaryGrass
--surface-preset /YourArt/Terrain/Fairway --surface-preset /YourArt/Terrain/Green
```

The script backs up the saved map and terrain definition under `Saved/TerrainBackups`, calls
`OpenGolfTerrainBuilder.RebuildCourseGrass`, waits for PCG, bakes, then saves. Only the
previous course grass layer is replaced, and a failed or empty placement keeps it. Save wanted
edits first so the backup includes them. Completion is logged as
`OPEN_GOLF_COURSE_GRASS_COMPLETE` with instance counts per mesh. Each `--surface-preset`
swaps the material of every generated surface of its type and updates the matching region
presets, so a later terrain rebuild keeps it; its material must carry that surface's physical
material. Course-specific paths belong in the invocation, not in the shared generators.
`Scripts/InspectCourseSurfaces.py` lists the open map's surface materials and grass counts
without changing anything. A full `RebuildVegetation` removes the layer, so apply it again
afterwards.

Validate with `UOpenGolf.Course.Terrain.TiledGrass` and `UOpenGolf.Course.Terrain.BoundaryGrass`,
saved-terrain contact and rollout checks, then `UOpenGolf.Visual.CoastalTerrain`. Compare
stationary tee/edge/close grass-on and grass-off sequences using
`Scripts/grass_stability.py --profile coastal`, and inspect slow camera movement. Keep
camera, resolution, AA and editor count the same before and after.
