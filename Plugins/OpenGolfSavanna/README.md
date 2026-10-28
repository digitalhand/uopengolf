# Open Golf Savanna

Reusable open artwork for the course kit. See [NOTICE.md](NOTICE.md) for source credits
and licences. Course layout and physics remain in the course kit.

Content uses type-first folders: `Meshes/<Category>`, `Materials/<Category>` and
`Textures/<Category>`, with shared parent materials in `Materials/`. Blender projects and
downloaded originals live outside the repository under the Blender art root
(`UOPENGOLF_BLENDER_ROOT`, default `~/Code/3D/blender/uopengolf`).

## Rough grass tiles

Grass Medium 01 (Poly Haven, CC0) becomes a family of dense tiles, like a commercial grass
pack's `Dense` meshes, which fit edge to edge without gaps:

| Meshes (`Meshes/Grass/Medium/`) | Footprint | Use |
| --- | --- | --- |
| `SM_GrassMedium_30cm_1m_A`–`D` | 1 m square | Open rough |
| `SM_GrassMedium_30cm_50cm_A`–`B`, `_25cm_A`–`B` | 0.5 m, 0.25 m square | Near play edges and on curved ground |
| `SM_GrassMedium_30cm_Edge_A`–`B` | 1 m along the edge, 0.25 m deep | Shorter strip along play outlines |

Every tile holds one tuft per 1/16 m cell, so all sizes have the same density. To rebuild:

```sh
python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py grass_medium_01 leafy_grass
blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportPolyHavenGrass.py -- [--regenerate]
```

The exporter opens `<art root>/Grass/Grass.blend`, where tiles can be edited, and only
rebuilds it with `--regenerate`. Then in the editor run
`Scripts/ImportSavannaAssets.py <project>/Saved/AssetImport/OpenGolfSavanna/Grass/export.json`.
The export also records the blades' average colour, which the ground under them is tinted to.

The exporters write 8-bit PNGs through `blender/png8.py`, which sets each image's colour space
when it is loaded. To update only the textures after a re-export, add `--textures-only` to
`ImportSavannaAssets.py` or `ImportNatureEssentials.py`. It replaces the textures in place and
leaves material instances (and any values tuned in them) and meshes alone. Texture changes need
the shimmer check in AGENTS.md.

## Course grass presets

`Scripts/BuildSavannaGrassPresets.py` (editor) creates, in `Terrain/`:

- `DA_RoughGrassDense`: tiled 1, 0.5 and 0.25 m squares covering the rough from 0 to 12 m
  from play, kept 0.4 m clear of play outlines. Beyond it the ground stays open; scattered
  tiles there read as spots on bare soil.
- `DA_BoundaryGrass`: edge strips 0.15 m outside every fairway, fringe, green, tee and
  bunker outline.

Apply them with the course kit's `ApplyCourseGrass.py`; see
[Course grass](../OpenGolfCourse/README.md#course-grass). Rebuilding the presets resets
their starting settings; copy a preset to tune it for another course.

## Surface textures

`Scripts/FetchAmbientCG.py Grass008 Ground093C` downloads and verifies the pinned CC0
archives (run it in a system terminal; if the host refuses the script, download the 2K-JPG
zip in a browser and pass `--archive`). Then run `Scripts/BuildSurfaceTextures.py` in the
editor. It builds one shared parent, `Materials/M_SavannaSurface`, and for each surface an
instance and a surface preset:

| Surface | Texture | Instance | Preset |
| --- | --- | --- | --- |
| Fairway | Grass008, 110 cm tiling, mowing stripes | `Materials/Fairway/MI_FairwayGrass008` | `Terrain/DA_FairwayGrass008` |
| Green | Grass008, finer, lighter, no stripes, little macro variation | `Materials/Green/MI_GreenGrass008` | `Terrain/DA_GreenGrass008` |
| Bunker | Ground093C sand | `Materials/Bunker/MI_BunkerGround093C` | `Terrain/DA_BunkerGround093C` |

Each instance keeps its surface's physical material (`PM_Fairway`, `PM_Green`, `PM_Sand`),
so play does not change. Normal detail fades with distance and roughness stays high.
The texture keeps its own colour detail: it is scaled so its mean (`TextureMean`, measured
from the pinned map) lands on `TargetColor`. `DetailStrength` blends from the flat target (0)
to the full texture (1). The instances also expose texture size, normal strength, macro
variation, roughness and mowing contrast. Apply a surface with `ApplyCourseGrass.py --surface-preset`.

Rendered acceptance and performance must be measured in the target course after application.
A successful asset import alone is not evidence of coverage or shimmer stability.

## Desert ground

`Scripts/BuildSavannaGround.py` (editor) builds `Materials/M_SavannaGround`, the ground outside
play: Poly Haven `red_sand` with noise-shaped Rock029 (ambientCG) patches, and the Leafy Grass
rough near play through a per-course mask texture. Fetch the sources first:

```sh
python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py red_sand leafy_grass
python3 Plugins/OpenGolfSavanna/Scripts/FetchAmbientCG.py Rock029
```

Each texture is scaled so its mean lands on a target colour (`SandTarget`, `RockTarget`), so a
sand can be swapped without retuning the rest. A course instance sets `CourseMask` (white near
play), `MaskBounds` (the mask's terrain-local minimum and size in metres), and `RoughLow` /
`RoughHigh` for the rough's shade; `NativeOnly` = 1 drops the rough for ground outside the course.
`RockAmount` sets how much of the desert is rock. Poly Haven's 16-bit PNGs import washed out, so
the sand uses its 8-bit JPG maps.

## Understory (Nature Essentials)

Grass tufts, dead moss, rocks, oak seedlings and leaf-litter patches for the ground under trees,
adapted from "Realistic Nature Essentials" by CG Geek (CC BY 4.0, attribution required; see
[NOTICE.md](NOTICE.md#understory-nature-essentials-cc-by-40)). Content is in the
`NatureEssentials` folders under `Meshes/`, `Materials/` and `Textures/`.

1. Edit `<art root>/NatureEssentials/NatureEssentials.blend` and export:

   ```sh
   blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportNatureEssentials.py
   ```

   `-- --regenerate` rebuilds the project from the purchased pack (`--source PACK.blend` or
   `UOPENGOLF_NE_PACK`) and discards edits.
2. In the editor run `Scripts/ImportNatureEssentials.py` (`--materials-only` keeps the meshes;
   `--textures-only` replaces only the textures and keeps tuned material instances).

Cards and rocks use `M_SavannaCard` and `M_SavannaSolid`. The grass uses
`M_SavannaUnderstoryGrass`, dry by default; a course can set `CourseMask`, `MaskBounds`,
`TerrainFrame` and `UseCourseMask` = 1 to green it near play. The rule tests run from
`Scripts/blender/` with `python3 -m unittest test_ne_rules`.

## Savanna range

`Scripts/BuildSavannaRange.py` turns a range map into the savanna course, or rebuilds it.
With no arguments it converts the public range, `/Game/UOpenGolf/Maps/UOpenGolf`, and writes
course assets to `/Game/UOpenGolf/Terrain/Savanna`; `--map`, `--course` and `--generated` select
another course. Build the savanna assets above first, then open the map and run it in the editor.
The script:

1. backs up the map and its terrain definition under `Saved/TerrainBackups/*-savanna-range`;
2. copies the definition to `<course>/DA_SavannaRange` the first time and switches the map to it;
3. moves the bunker onto the approach, sets the Grass008 and Ground093C surfaces, builds the
   course mask and ground instances (`MI_SavannaRough`, `MI_SavannaNative`, `DA_SavannaRough`),
   and places 36 seeded jacaranda patches (`savanna_layout.py`). Scattered grass presets are dropped;
4. rebuilds the terrain, trees, wood collision (`savanna_trees.py`) and understory
   (`understory.py`), then saves.

The importer sets the tree's Nanite shape preservation to Preserve Area. Without it, simplified
leaves shrink below a pixel at range: distant crowns look bare and shimmer under TSR.

Tree ball contact uses two hidden helpers exported with the tree by `blender/ExportJacaranda.py`:
`SM_JacarandaCollision` (the trunk and main limbs, triangle collision) blocks the ball, and
`SM_JacarandaCanopy` (one convex hull around the branches and leaves) slows a ball flying through
the crown without blocking it. To update them on an existing course, reimport only the helpers with
`ImportSavannaAssets.py <export.json> --meshes=SM_JacarandaCollision,SM_JacarandaCanopy`, then run
`Scripts/ApplyTreeCollision.py [--canopy]` on the open map; it backs up, applies, checks ball-sized
sweeps and saves. `Scripts/PlayTreeShots.py` plays trunk and canopy shots at one tree during Play.

Then place the course grass with the course kit's `ApplyCourseGrass.py`; the completion line
`OPEN_GOLF_SAVANNA_RANGE_COMPLETE` prints the command. The understory alone can be reapplied
with `Scripts/understory.py` on the open map. Layout rules are pure Python with tests
(`python3 -m unittest test_savanna_layout test_understory_layout`).

## Maintained turf

The original short-grass pipeline and its scoped import/apply commands are described in
[Maintained turf workflow](../../Docs/TurfWorkflow.md). Validation status is in
[Phase status](../../Docs/PhasesStatus.md).
