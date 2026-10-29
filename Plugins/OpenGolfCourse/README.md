# Open Golf Course

Reusable course kit for uopengolf maps: surface materials that declare OpenFairway physics
surfaces, terrain generation with vegetation scatter, a flagstick whose cloth flag flies in the wind
and a marked golf ball. The range uses it, and new
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
- [Vegetation wind](#vegetation-wind)

## Contents

| Asset | Purpose |
| --- | --- |
| `Surfaces/PM_<Surface>` | One `UOpenFairwayPhysicalMaterial` per surface: Fairway, FairwaySoft, Rough, Firm, Green, Sand, Fringe |
| `Surfaces/M_Turf` | Parent turf material: world-aligned grass textures, mowing bands, a two-color palette |
| `Surfaces/MI_Fairway`, `MI_Rough`, `MI_Green` | Turf instances; each points at its physical material, so assigning one sets look and physics together |
| `Surfaces/MF_MowStripes` | Mowing-band mask for any direction and band width, filtered against moire |
| `Surfaces/T_Grass_*` | ambientCG Grass 004 color, normal and roughness maps |
| `Flagstick/SM_Flagstick`, `M_Pole` | Seven-foot, half-inch pole |
| `Flagstick/M_FlagCloth`, `MI_Flag_Red` | Two-sided flag cloth material (`FlagColor`, `FlagRoughness`) and its red instance |
| `Ball/SM_GolfBall`, `M_GolfBall`, `MF_BallMarking` | Dimpled ball (three LODs) with an alignment line and a logo dot |
| `UOpenGolfFlagstickComponent` (C++) | The pole plus a simulated 20 x 14 inch cloth flag (`FOpenGolfFlagCloth`); no collision; `SetWind` flies it |
| `UOpenGolfWindSubsystem` (C++) | A world's wind (10 m vector, air density, height profile); writes `Wind/MPC_Wind` |
| `OpenGolfBeaufort.h` (C++) | The Beaufort land scale, with the flag bands the cloth is held to |
| `Wind/MPC_Wind`, `MF_VegetationWind` | Vegetation World Position Offset in the world's wind; see [Vegetation wind](#vegetation-wind) |

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
0.08 detail, with no mowing stripes (`StripeContrast` 0).

Bands are procedural, so they get no mip filtering. Their edges widen with the pixel
footprint, and their contrast fades to the mean as bands narrow toward the horizon, which
keeps them from aliasing into moire under temporal anti-aliasing.

### Flagstick

Add an **Open Golf Flagstick** component to any actor, with its origin on the ground at the
hole. The component draws the pole. Its `FlagCloth` child, a procedural mesh, draws the flag,
which `FOpenGolfFlagCloth` simulates every tick. Recolor it by setting `FlagMaterial` to another
`M_FlagCloth` instance, as `MI_Flag_Red` is.

Call `SetWind(WindVelocityMps, AirDensityKgM3)` whenever the wind changes. The range calls it
with the ball's 10 m wind reduced to the flag's height by the log wind profile
(`OpenFairway::WindProfile` at `FlagCentreHeightCm`), about 72% of the authored speed, through the
ball's reference height and roughness. `GetMotion()` reports the flag's averaged droop and flap rate
while it simulates. It samples every cloth substep, so the flap rate does not depend on the frame
rate; droop reads 0 when a gale flaps the fly end above the hoist.

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
  pressure across the cloth (`NormalForceCoefficient`, 0.7) and skin friction along it
  (`FrictionCoefficient`, 0.015), from the air's speed relative to the cloth.
  - The pressure coefficient is below an isolated flat plate's 1.2 because each triangle meets
    free-stream air, with no shelter from the cloth upwind of it. 0.7 holds the flag to the
    Beaufort bands (`UOpenGolf.Course.FlagBeaufort`, below); with 1.2 the flag read as extended a
    force early, at 6–7 mph.
  - A flag in its own plane feels only friction, so the wind holds it out through the drag of its
    own flapping, as on a real flag.
  - Gusts of about 15% in speed and 8 degrees in direction drift down the flag.
- **Sleeve.** The hoist sleeve turns downwind on the round pole over about a second, and stays
  put in still air.
- **Start.** The flag settles in calm air for two seconds when it is created, so it never
  starts sticking out.

Measured on the solver with the wind applied at the flag (the `UOpenGolf.Course.FlagCloth` test
checks the same behavior):

| Wind at the flag | Middle of the flag | Free edge |
| --- | --- | --- |
| Still air | Hangs about 78 degrees below horizontal, at rest | Top corner about 47 cm below the pole top |
| 5 mph | Lifts to about 32 degrees | Flaps |
| 10 mph | Flies out, about 15 degrees | Flaps about 5 cm |
| 20 mph | Nearly straight, about 6 degrees | Flaps |

Against real wind, `UOpenGolf.Course.FlagBeaufort` sweeps the authored 10 m wind from 0 to 40 mph
through the range's height profile and checks each speed against its Beaufort force's flag band
(`OpenGolfBeaufort.h`): hanging at force 0–1, stirring at 2 (4–7 mph), extended at 3 (8–12 mph)
and flying straight out from 5. The flap rate must keep a near-constant Strouhal number
(flaps × flag length / wind, 0.1–1, as measured on flapping flags), so it rises in proportion to
the wind, and at 30–40 mph it must read the same at 30 fps as at 60. A 60 mph gale, which only the
console variable reaches, must stay finite and in its band; there the cloth stretches about 11%, past
the sweep's 8% limit. The sweep is written to `Saved/Automation/FlagWind/sweep.csv`.

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
`SM_GolfBall.blend`, `SM_Flagstick.blend`, and `Coastal/` for the coastal grass. The putting-disc,
green-surround and grass-clump sources there served the retired flat range and are no longer exported.
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

Archive SHA-256 checksums pin the downloads in `Scripts/PrepareCourseAssets.py`. See the
repository's [third-party notices](../../THIRD_PARTY_NOTICES.md). Material graphs are
original MIT contributions.

## Tests

`UOpenGolf.Course.Assets` checks the following:

- A physical material exists for every surface.
- `MI_Fairway`, `MI_Rough` and `MI_Green` play as Fairway, Rough and Green and share `M_Turf`;
  the green has no stripes.
- The flagstick mesh is the pole alone, with the right height and no collision; its flag material is
  two-sided, with no world-position offset.
- The ball material exposes its marking parameters.

`UOpenGolf.Course.FlagCloth` runs the cloth headless:

- Still air: it hangs and comes to rest.
- 10 mph: it flies downwind and flaps.
- 5 mph sags more than 10 mph, and 20 mph flies higher.
- Nothing stretches more than a few percent.
- 30 and 144 fps agree.

`UOpenGolf.Course.FlagBeaufort` holds the cloth to the Beaufort flag bands from 0 to 40 mph and in a 60 mph gale (see
[Flagstick](#flagstick)). `UOpenGolf.Course.WindSubsystem` checks a world's wind: calm by default,
the log height profile, gusts that stay in place when the speed changes, and `MPC_Wind` once built.
`Scripts/test_vegetation_wind.py` checks the vegetation motion: calm is exactly still, each layer
starts at its Beaufort force, motion grows with the wind and stays inside the material bounds, grass
tiles meet without seams, and a tree's materials stay joined.

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
  vegetation_wind.py      Vegetation wind profiles, HLSL and its Python mirror (pure)
  vegetation_wind_assets.py  MPC_Wind, MF_VegetationWind, add_vegetation_wind, apply_profile
Source/OpenGolfCourse/
  Public/OpenGolfCourse.h               Asset paths
  Public/OpenGolfFlagstickComponent.h   Flagstick component (pole + cloth flag)
  Public/OpenGolfFlagCloth.h            Flag cloth solver and its droop/flap readings
  Public/OpenGolfWindSubsystem.h        A world's wind and MPC_Wind
  Public/OpenGolfBeaufort.h             Beaufort land scale and flag bands
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
without changing anything. A full `RebuildVegetation` keeps the layer; after a terrain change,
place it again with `RebuildCourseGrass` so it follows the new ground.

Validate with `UOpenGolf.Course.Terrain.TiledGrass` and `UOpenGolf.Course.Terrain.BoundaryGrass`,
saved-terrain contact and rollout checks, then `UOpenGolf.Visual.CoastalTerrain`. Compare
stationary tee/edge/close grass-on and grass-off sequences using
`Scripts/grass_stability.py --profile coastal`, and inspect slow camera movement. Keep
camera, resolution, AA and editor count the same before and after.

## Vegetation wind

Trees and tall grass move in the same wind as the ball and flag. Each world has a
`UOpenGolfWindSubsystem`; a source such as the range calls `SetWind` with the 10 m wind, and the
subsystem writes `Wind/MPC_Wind`. Vegetation materials read it through `MF_VegetationWind`, whose
motion is `vegetation_wind.WIND_HLSL`:

Wind pushes a plant over; it does not swing it to and fro. Each layer is a damped spring loaded by
drag, and its deflection is always downwind:

- **Bend.** The whole plant leans about its base, growing with height. Points drop as they lean, so
  blades and trunks keep their length.
- **Limbs.** Branches stream downwind, more toward the crown's edge and top.
- **Leaves.** Leaves and blade tips stream a little and flutter. Flutter is the only symmetric
  motion, and it is at most about a centimetre.

Drag grows with the square of the wind. A layer's load is `C = (u / U½)²`, and it bends `C / (1 + C)`
of its largest deflection, so it saturates as plants streamline in a gale (de Langre 2008, *Annu.
Rev. Fluid Mech.* 40). `U½` is the 10 m wind that bends the layer half way: its stiffness against its
drag area. That is why grass goes over before branches, and branches before trunks.

Gusts are frozen turbulence carried downwind at the wind's speed: four waves, 80 to 5 m long. Their
size is the turbulence intensity over the course's roughness, `1 / ln(z / z0)` (EN 1991-1-4 §4.4),
taken at the plant's height but no lower than 2 m. A gust passes a point at `speed / wavelength` Hz.
Each layer answers it as a damped oscillator with its own natural frequency and damping, and this is
where weight shows:

- A heavy trunk follows only the long gusts and sways near its own frequency.
- Light grass follows almost every gust, so gusts roll across a field as waves (honami; Py, de Langre
  and Moulia 2006, *J. Fluid Mech.* 568).

Gusts add to the load: a surge pushes a plant further, and a lull lets it come part of the way
back, never upwind of upright. Calm air is exactly still.

Everything is a smooth function of the rest position and instance transform. Trunk, branch and leaf
materials of a tree compute the same bend and sway, so they stay joined; leaves add only their own
stream and flutter, about 4 cm at most. Neighbouring grass tiles meet without seams. Trees
take their gust at their base, so each tree moves as one. **Never hash a rebuilt position.** The base
of a tree is rebuilt at each vertex in float32 and differs by hundredths of a millimetre. A hash turns
that difference into a different phase for every vertex, which makes leaves flicker; this happened
before 2026-10-29.

A **profile** (`vegetation_wind.WindResponse`) is data. Each `Layer` has:

- its largest deflection (`max_cm`, or `max_deg` as a lean angle for grass)
- `half_mps`
- its natural frequency
- its damping ratio

`Leaves` sets the flutter and streaming. Each profile also sets how the bend grows with height, and
whether gusts vary across the plant (grass tiles) or come once per plant (trees). The values cite
their sources in `vegetation_wind.py`, and the tests hold them to the Beaufort land descriptions.

Deflections grow with the plant's size: a taller mesh or a larger instance moves farther in
proportion, so it leans the same angle. Lean angles (`max_deg`) are size-free. Centimetres describe
a plant of the profile's `size_cm`, or of the mesh's own height when that is 0. `BroadleafTree`
describes a 9 m tree: the range places its 19.6 m jacaranda mesh at 0.30–0.62 scale.

| Profile | Bend: max, U½, natural frequency, damping | Limbs | Leaves |
| --- | --- | --- | --- |
| `BroadleafTree` (a 9 m jacaranda) | 150 cm at the top, 25 m/s, 0.35 Hz, 0.15 | 40 cm, 12 m/s, 1.2 Hz, 0.15 | 0.8 cm flutter at 2.5 Hz, 3 cm stream, 4 m/s |
| `TallGrass` (Grass Medium) | 60° lean, 7 m/s, 1.5 Hz, 0.25 | none | 0.6 cm at 3 Hz |
| `UnderstoryGrass` (Nature Essentials) | 55° lean, 10 m/s, 1.0 Hz, 0.25 | none | 0.5 cm at 2 Hz |

Resulting mean lean, with the gust range in brackets (the jacaranda at 9 m):

| 10 m wind | Jacaranda crown top | Tall grass | Understory grass |
| --- | --- | --- | --- |
| 10 mph | 4 cm (2–9) | 14° (7–25) | 8° (3–17) |
| 20 mph | 17 cm (8–30) | 32° (20–45) | 21° (11–32) |
| 30 mph | 32 cm (20–47) | 42° (30–51) | 32° (21–41) |

The material's `MaxWorldPositionOffsetDisplacement` is sized for winds up to 45 mph and for its
largest placed instance: about 236 cm for the jacaranda (scale 0.62), 50 cm for Grass Medium (scale 1.3)
and 100 cm for the understory grass (scale 1.5).

To give vegetation wind:

1. Call `vegetation_wind_assets.add_vegetation_wind(material)` in its parent material's builder.
   This adds World Position Offset behind the `VegetationWind` static switch, off by default, so the
   parent's other users (rocks, litter) compile without it. A `LeafFlutter` switch, on by default,
   lets trunks and branches leave flutter to their leaves.
2. Choose a profile, or add one to `vegetation_wind.PROFILES`.
3. Call `apply_profile(instance, profile, height_cm, crown_radius_cm, flutter, max_scale)` on each
   material instance (size from `mesh_size_cm`; `max_scale` is the largest scale its meshes are
   placed at). This also sets the instance's `MaxWorldPositionOffsetDisplacement`, which Nanite
   needs; a larger instance would have its motion clamped.
4. Let the placed meshes run it: set `bWindMotion` and `WindMotionDisableDistanceM` on the
   vegetation preset, or call `enable_wind_motion(component, distance_m)` on components placed
   another way.
5. Run the shimmer checks in `AGENTS.md`, at 0 mph (unchanged from before) and in wind.

The savanna kit keeps its list in `vegetation_wind_assignments.py` and applies it with
`ApplyVegetationWind.py`.
