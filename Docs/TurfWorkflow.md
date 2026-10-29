# Blender grass: sources, workflow and handoff

Read this before changing the maintained turf. The current editable source is
`<art root>/CutTurf/CutTurf.blend`. Do not start a new grass project or regenerate the source
just to inspect it. All grass in this workflow is original project artwork.

## Current state — 2026-10-27

The first pass was rejected as sparse and regular. The second, denser pass then showed a colour
disc around the camera, jagged mowing edges and a square tee inside a round grass clearance
(causes in [Phase status](PhasesStatus.md#maintained-turf-shared-field-smooth-edges-and-square-tee--2026-10-27)).
The current revision fixes those and is applied to the public range:

- **One turf field for everything.** Ground, rough and blades evaluate the same world-space shader
  (`turf_field.py`), driven by `T_CourseSurfaceField`, a signed-distance texture of the play
  outlines. A seam between ground materials shows no colour change, and a blade matches the
  ground under it. Layers: rough, first cut, fairway, bunker lip, collar, green, tee.
- **Smooth outlines.** Play outlines are curves through their authored corners
  (`outline_smoothing.py`); physics, ground meshes and turf follow the same curve.
- **Square tee.** `LevelTeeHalfSizeM` (2.5 × 2.5 m) levels the ground and clears grass. The field
  draws a lighter, finely mown tee; the range has no painted tee box.
- **Light, near-only blades.**
  - Fairway and collar blades are one tapered quad, 4–4.4 mm wide. First cut keeps two segments.
  - A 50 cm fairway tile has 21,430 triangles, down from 85,512.
  - Blades draw to 10 m (first cut 14 m), with tone variation fading out between 4 and 7 m.
  - There is no vertex motion.

Nominal mowing heights remain 15 mm fairway, 30 mm first cut and 20 mm collar. The green uses a
4 mm blade family for texture baking only. Owner visual acceptance is pending.

## Agent entry points

`CLAUDE.md`, `CODEX.md` and `AGENTS.md` point to this handoff. This checkout ignores `CLAUDE.md`
and `AGENTS.md`, as well as `CLAUDE.local.md`; their local edits do not appear in normal Git
status. `CODEX.md` and this workflow document are not ignored and can carry the handoff in Git.
The ignore policy has not been changed. A new checkout must not assume those local instruction
files or the external Blender source are present. Restore Git LFS assets before opening Unreal.

## Find the existing work

The art root is resolved by `Plugins/OpenGolfCourse/Scripts/art_root.py`: an explicit
`--art-root`, then `UOPENGOLF_BLENDER_ROOT`, then `~/Code/3D/blender/uopengolf`. Exact paths on
this machine are in the ignored `CLAUDE.local.md`, which both agents can read.

| Location | Contents |
| --- | --- |
| `<art root>/CutTurf/CutTurf.blend` | Current editable source: the denser second pass |
| `<art root>/CutTurf/CutTurf-before-fullness.blend` | Explicit backup of the sparse first pass |
| `<art root>/CutTurf/CutTurf.before-subdivision.blend` | Earlier source backup, before tile-size and height-control changes |
| `<art root>/CutTurf/ExportsBeforeFullness/` | First-pass FBX files, texture exports and manifest |
| `<art root>/CutTurf/Bakes/` | Current floating-point height-bake intermediates |
| `<art root>/CutTurf/BakesBeforeFullness/` | First-pass height intermediates |
| `Saved/AssetImport/OpenGolfSavanna/CutTurf/` | Current Unreal-ready FBX/PNG files, `export.json`, mask and mask-source JSON |
| `Saved/Automation/CutTurf/Fullness/` | Matching `Fairway-before.png` and `Fairway-after.png` Blender previews |
| `Saved/TurfAssetBackups/20261028-024936-before-fullness/` | Complete Unreal asset backup from before the fuller-grass import |
| `Saved/TerrainBackups/` | Map and terrain-definition backups from the first application and repeat bake |
| `Saved/Automation/CutTurf/` | Inspection JSON, repeat fingerprints, measurements and test results |

The later `Saved/TurfAssetBackups/20261028-025148-before-fullness/` folder was made for an
import retry after textures had already changed. Do not use that mixed-state folder as the
complete first-pass rollback. The `024936` folder predates the first import attempt.

The `.blend1` file is Blender's rolling backup, not the canonical source or a substitute for
an explicit revision backup. `Saved/` and the external art root are local work; they are not
committed. A fresh checkout needs the external source copied from the owner's art storage or
a newly generated source. Keep Blender files and full-resolution intermediates outside Git.
Only Unreal-required assets and texture sources at 2K belong in the repository.

The live Unreal assets are in the public plugin:

- `/OpenGolfSavanna/Meshes/CutTurf/<Family>/SM_Turf<Family>_<Footprint>_<Variant>`
- `/OpenGolfSavanna/Textures/CutTurf/<Family>/T_Turf<Family>_<Role>`
- `/OpenGolfSavanna/Materials/CutTurf/` and `/OpenGolfSavanna/Materials/M_CutTurfSurface`
- `/OpenGolfSavanna/Terrain/CutTurf/`

The public range, `/Game/UOpenGolf/Maps/UOpenGolf`, is the project's only map. Confirm the
current map and Play state before doing anything. This work does not modify the terrain layout,
physical surface types or ball tuning. Scripts that rebuild course grass or ground meshes delete the
generated folders they replace once the map is saved.

## Scripts and controls

All paths below are under `Plugins/OpenGolfSavanna/Scripts/`.

| Script | Purpose |
| --- | --- |
| `blender/turf_rules.py` | Pure Python blade shapes, loose clumps, filler, density, colours and repeatable seeds |
| `blender/ExportCutTurf.py` | Create or open the source and export its meshes and manifest |
| `blender/BakeCutTurf.py` | Bake unlit colour, filtered height-derived normals and roughness at 2K |
| `blender/PreviewCutTurf.py` | Render matching before/after patches, using the explicit first-pass backup |
| `BuildCutTurf.py` | Import meshes/textures and build the turf materials and presets |
| `ApplyCutTurf.py` | First installation of the turf on the public range; `--export-mask` writes `course-field.json` |
| `SmoothCourseOutlines.py` | Smooth the play outlines, rebuild only the ground meshes, re-place course grass |
| `FixTurfTransitions.py` | Import the course field, build the field materials, configure every instance |
| `turf_field.py` / `turf_shading.py` | Shared field shader and per-family profiles / wiring and instance configuration |
| `turf_mask.py` | Bake `T_CourseSurfaceField` (signed distances to fairway, green, fringe, sand); requires NumPy |
| `../../OpenGolfCourse/Scripts/outline_smoothing.py` | Curves through authored corners; crossing check |
| `blender/test_turf_rules.py` | Footprints, repeatability, coverage, triangle budget and blade width |
| `test_turf_mask.py` | Signed distances, clamping, zero crossings, layer order and file format |

The source contains three mesh families: `Fairway`, `FirstCut` and `Collar`. Each has four
variants (`A`–`D`) of five footprints: 50, 25, 12.5 and 6.25 cm squares, plus 50 by 5 cm edge
strips. This gives 60 meshes. Objects share the same origin for export; isolate one object
when inspecting it. Looking at all 60 overlaid objects is not a valid density preview.

Change `SURFACES` in `turf_rules.py` for nominal height, blade width, clump spacing, blades per
clump, filler fraction and base colour. The `Cut Turf` Geometry Nodes modifier adjusts height
on existing source objects. Most shape and distribution controls are in the Python generator,
not exposed as Geometry Nodes sockets. Direct mesh edits are preserved by a normal export.
Changing generator settings requires rebuilding the source after backing up artist edits.

Vertex colour is named `BladeHeight` for compatibility. Its channels now carry:

- Red: normalized blade height. The blades no longer use vertex motion, so nothing reads it now.
- Green: per-blade tone variation.
- Blue: clump variation.
- Alpha: one.

Preserve those channels in FBX import. The blade material uses upward world shading normals,
high roughness and no grass shadows or wind animation. Tips can overhang neighbouring tiles;
do not clamp them into straight walls. Keep green `StripeContrast` at zero.

## Blender MCP and source editing

Blender 5.0.1 and MCP for Blender 2.2.4 were used. MCP is installed at **user scope for both
agents**, using the same pinned server and matching enabled Blender add-on:

- Codex registration: `~/.codex/config.toml`, server `blender`.
- Claude Code registration: `~/.claude.json`, server `blender`.
- The project `.mcp.json` does not own this Blender registration.

Read local notes for executable paths and connection settings. Reuse the existing Blender
session and discover its tools. `get_scene_info`, `execute_blender_code` and `look` were verified.
A newly registered server may require reconnecting the agent session. Do not assume acceptance
of a tool call means the export or render completed: inspect its output and files.

Do not use `bpy.ops.wm.read_factory_settings()` through MCP: it disabled the add-on and stopped
the connection during this work. Loading another `.blend` can also break the connection.
The exporter already supplies an explicit context for FBX export from an MCP callback.
Temporary `/tmp/turf_*.py` clients used during development are not part of the durable workflow;
use the configured MCP tools and these project scripts instead.

For source inspection, open `CutTurf.blend`, inspect one mesh and its height modifier, then
render `PreviewCutTurf.preview('Fairway')`. The preview compares the current source against
`CutTurf-before-fullness.blend` with the same camera and light.

For a generator revision, first save an untouched, uniquely named copy of the current scene.
Then, through `execute_blender_code`, adapt this example with the actual project location:

```python
import bpy
import importlib
import sys
from datetime import datetime
from pathlib import Path

project = Path('/absolute/path/to/uopengolf')
sys.path.insert(0, str(project / 'Plugins/OpenGolfSavanna/Scripts/blender'))
sys.path.insert(0, str(project / 'Plugins/OpenGolfCourse/Scripts'))
from art_root import art_root

source = art_root() / 'CutTurf/CutTurf.blend'
if Path(bpy.data.filepath) != source:
    raise RuntimeError('Open the existing CutTurf.blend before rebuilding it')
backup = source.with_name('CutTurf-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.blend')
bpy.ops.wm.save_as_mainfile(filepath=str(backup), copy=True)

import turf_rules
import ExportCutTurf
importlib.reload(turf_rules)
importlib.reload(ExportCutTurf)
ExportCutTurf.build_source(source)
```

`build_source()` rebuilds the generated turf objects and saves the source. It does not export
FBX. To export the **open current source without reloading it**, use:

```python
previous_args = sys.argv
try:
    sys.argv = ['blender', '--', '--current']
    ExportCutTurf.main()
finally:
    sys.argv = previous_args
```

For an ordinary shell export that preserves the saved source:

```sh
blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportCutTurf.py
```

`--regenerate` rebuilds and replaces the source; it is not an inspection or routine-export
option. Preserve a backup first. Never execute the exporter's final `os._exit()` block in the
interactive Blender process; import the module and call `main()` instead.

## Bake and refresh Unreal assets

Open the current saved source before baking. Reload `turf_rules` and `BakeCutTurf` after code
changes, then call `BakeCutTurf.bake()` separately for `Fairway`, `FirstCut`, `Collar` and `Green`.
The first three use the current source meshes. Green is generated only for its fine texture
bake. The colour bake uses emission to exclude directional lighting; normals come from a
filtered height bake. Height EXRs stay in the external art root. Re-bake after geometry changes.

In Unreal, with Play stopped, run scripts using `py "<project>/..."`:

| Command | Use |
| --- | --- |
| `BuildCutTurf.py --refresh` | Reimport revised meshes and maps; rebuild the blade material; set blade draw distances; retain surface presets and placement |
| `BuildCutTurf.py --textures-only` | Reimport maps only; retain meshes and tuned instances |
| `BuildCutTurf.py` | Full initial build; restores starting material and grass preset settings |
| `ApplyCutTurf.py` | Apply/rebuild the course layer when first installing it or changing placement/band width |

Use `--refresh` for the current fullness revision. Back up existing, possibly untracked Unreal
assets before reimporting them. Do not run the whole savanna or course-kit generator for this
scoped update. No terrain rebuild or new PCG bake is needed when only existing mesh assets and
textures change. `OPEN_GOLF_CUT_TURF_REFRESH_COMPLETE` confirms the refresh script reached its end.

Whenever course outlines or the tee change, and after a refresh, rebuild the field. With the public
map open and Play stopped:

1. `SmoothCourseOutlines.py`, if outlines were edited. Wait for `OPEN_GOLF_OUTLINES_SMOOTHED`. It also
   writes `course-field.json`; otherwise run `ApplyCutTurf.py --export-mask`.
2. Bake the field from the project shell (about a minute):

   ```sh
   python3 Plugins/OpenGolfSavanna/Scripts/turf_mask.py \
     Saved/AssetImport/OpenGolfSavanna/CutTurf/course-field.json \
     Saved/AssetImport/OpenGolfSavanna/CutTurf/T_CourseSurfaceField.tga
   ```

3. `FixTurfTransitions.py`; wait for `OPEN_GOLF_TURF_FIELD_COMPLETE`.

`BakeCutTurf.bake()` records `texture-means.json`; the field needs it. The fairway channel reaches
2.5 m, so first-cut widths are limited to 2.2 m. Do not use `UOpenGolfTerrainBuilder.BuildTerrain` to
rebuild after outline edits: it destroys every generated actor, including trees and vegetation.
`RebuildSurfaceMeshes` replaces only the ground meshes.

## Validation and remaining work

Python source checks:

```sh
python3 -m unittest discover -s Plugins/OpenGolfSavanna/Scripts/blender -p "test_*.py"
python3 -m unittest discover -s Plugins/OpenGolfSavanna/Scripts -p "test_*.py"
python3 -m unittest discover -s Plugins/OpenGolfCourse/Scripts -p "test_*.py"
```

The material builders must use the default output (`''`) from `MaterialExpressionVertexColor`;
`RGB` is not a named output on that node. The course field is an uncompressed vector texture,
so its sample uses the Linear Color sampler. Do not confuse intermediate compile warnings while
rebuilding a graph with its final compilation result.

After any turf edit, run `UOpenGolf.Visual.CoastalTerrain` in Play with one editor and a fixed
layout. Compare grass-on and grass-off views (0/1, 4/5, 6/7); the cut turf should match its
ground within a couple of sRGB levels. Analyze with `Scripts/grass_stability.py --profile coastal`,
and compare second differences in the View 9 pan for motion flicker. Blender previews are not
Unreal shimmer or performance evidence.

Results for the current revision, with its limits, are in
[Phase status](PhasesStatus.md#maintained-turf-shared-field-smooth-edges-and-square-tee--2026-10-27).
The captures are `AfterTurfField-20261028-042858`, compared against `BeforeEdgeRepair-20261028-032124`,
under `Saved/Automation/CoastalReview`. Rendering cost is not yet measured: the earlier frame
probe was frame-capped. `UOpenGolf.Range.RolloutSurfaces` fails because its flat-range
assumptions send the fixed shot into the saved coastal bunker; do not alter physics to make that
layout match the flat fixture.

Record final results and limits in `Docs/PhasesStatus.md`, and update the asset roadmap when
the status changes. Owner review is required before claiming visual
acceptance. See [AGENTS.md](../AGENTS.md) for the shared rules.

## Maintained turf relief and quieter palette — 2026-10-27

The maintained-only palette path is `FixGrassPalette.py --maintained-only`. It leaves the
saved tall-rough parent and instance untouched. Ground relief now follows the terrain normal;
the first cut uses its baked normal instead of blending to a flat upward vector. Fairway,
collar and green ground use their existing `TurfNormal` texture with the shared relief builder.
`FairwayReliefStrength` is the shared ground parameter (0.55, or 0.18 for green); first cut
uses `FirstCutReliefStrength` (0.55). Relief fades from 4 to 18 m. Blades retain upward normals.

The normal bake slope scale is 0.07 and roughness varies from 0.92 to 0.96 with filtered blade
height. Ground roughness has a 0.90 floor. Colour variation uses smaller, screen-filtered
patches and restrained dry/lush weights, with baked colour detail at 1.35. Meshes, heights,
placement, draw distances and lighting are unchanged. Use the texture-only import, then the
maintained-only palette path to reproduce the result. Restore the existing source and bake
backups before undoing this revision; do not use a full generator to reset tuned assets.
