# Blender grass: sources, workflow and handoff

Read this before changing the maintained turf. The current editable source is
`<art root>/CutTurf/CutTurf.blend`. Do not start a new grass project or regenerate the source
just to inspect it. All grass in this workflow is original project artwork.

## Current state — 2026-10-27

The first pass was rejected as sparse and regular. The second, denser pass then showed a colour
disc around the camera, jagged mowing edges and a square tee inside a round grass clearance
(the shared-field and outline fixes are described below).
The current revision fixes those and is applied to the public range:

- **One turf field for everything.** Ground, rough and blades evaluate the same world-space shader
  (`turf_field.py`), driven by `T_CourseSurfaceField`, a signed-distance texture of the play
  outlines. A seam between ground materials shows no colour change, and a blade matches the
  ground under it. Layers: rough, fairway, bunker lip, collar, green, tee. The fairway's first cut
  was removed on 2026-11-23 (see [Fringe only around the green](#fringe-only-around-the-green--2026-11-23)).
- **Smooth outlines.** Play outlines are curves through their authored corners
  (`outline_smoothing.py`); physics, ground meshes and turf follow the same curve.
- **Square tee.** `LevelTeeHalfSizeM` (2.5 × 2.5 m) levels the ground and clears grass. The field
  draws a lighter, finely mown tee; the range has no painted tee box.
- **Light, near-only blades.**
  - Fairway and collar blades are one tapered quad, 4–4.4 mm wide. First cut keeps two segments.
  - A 50 cm fairway tile has 21,430 triangles, down from 85,512.
  - Blades draw to 10 m, with tone variation fading out between 4 and 7 m. (The removed first cut drew to 14 m.)
  - There is no vertex motion.

Nominal mowing heights remain 15 mm fairway and 20 mm collar; the 30 mm first-cut family is no longer
placed. The green uses a 4 mm blade family for texture baking only. Owner visual acceptance is pending. The rough band is
the 50 mm `Rough` family described in [50 mm rough](#50-mm-rough--2026-11-20).

## 50 mm rough — 2026-11-20

The Poly Haven tall rough (33–36 cm tufts in `DA_TurfTallRough`) and its 25 cm edge strip
(`DA_TurfRoughEdge`) read as weeds. A fourth CutTurf family, `Rough`, replaces both. It follows the
owner's reference, the tutorial "Blender: Fast & Easy Grass" (a hair-particle lawn): thin blades of
even height that stand nearly upright and lean over in their top third.

- **Blades.** `SURFACES['Rough']` in `turf_rules.py`:
  - 50 mm nominal height (±10%), 3.4 mm wide.
  - Three segments, bending towards the tip (`droop` 1.0). Tips reach about 0.4 of the height.
  - About 22,500 blades and 135k triangles per m², fewer than the 165–210k of the Poly Haven tiles.
  - The maintained families' meshes are unchanged; `test_turf_rules.py` pins their fingerprints.
- **Tiles.** 1 m, 50, 25 and 12.5 cm squares, variants A–D (`FOOTPRINTS`), and no edge strip. The
  band is about 8,800 m², so 1 m tiles keep placement well under the 100,000-point limit.
- **Colour.** Retuned on 2026-11-20 to the owner's reference screenshots: a dark olive rough, with first cut,
  fairway and green about equally light (see the [colour retune](#colour-retune--2026-11-20)). Rough Mid
  `435126` (`turf_palette.py`).
  - Rough blades darken towards a near-black thatch at the root and turn a lighter olive at the tip
    (`ROUGH_BLADE`), read from `BladeHeight` red.
  - The tip changes hue, not brightness, so blades seen from above match the ground under them.
  - `RoughDetailStrength` 2.0 stretches the baked blade-to-gap contrast for the reference's dark gaps.
- **Field.** Rough is now the turf field's lowest layer, with its own baked `T_TurfRough_Color`.
  - Every cut-turf material draws rough beyond the first cut; previously that was first-cut colour.
  - `M_CutTurfRough` blends the rough into the savanna ground by its `RoughWeight`, so the rough
    fades into the desert at 12–16 m.
  - `MI_TurfRough`'s `RoughLow`/`RoughHigh` follow the rough palette (`ROUGH_UNDERLAY`).
  - The rough texture uses the shared wrap sampler, because `M_CutTurfRough` already uses 16.
- **Placement.** `DA_TurfRoughGrass` (`BuildCutTurf.ROUGH_BAND`):
  - Runs 12 m from play and starts 8 cm from the fairway, fringe, sand and tee. Until 2026-11-23 it started at the
    first-cut width.
  - Tilt limit 2°, sink 3 mm, no wind.
  - Blades cull at 24 m; their tone and gradient fade out between 8 and 14 m.
  - The rough meets the fairway with no first cut or edge strip. The old presets stay on disk as a rollback.

Install it on a range that has the tall rough, with one editor, the public map open and Play stopped:

1. In Blender, back up `CutTurf.blend`. Then run `ExportCutTurf.build_source(source, families=('Rough',))`,
   which rebuilds only Rough and keeps the other 60 objects and their adjustments.
2. Export with `--current` (76 meshes), then run `BakeCutTurf.bake('Rough')` only.
3. Back up the CutTurf Unreal assets, then run `BuildCutTurf.py --refresh`.
4. Rebuild the field (`ApplyCutTurf.py --export-mask`, `turf_mask.py`, `FixTurfTransitions.py`). This
   rebuilds `M_CutTurfRough` with the rough layer.
5. `ApplyCutTurfRough.py` swaps `DA_TurfTallRough` for `DA_TurfRoughGrass`, removes the boundary
   preset and re-places the course grass. It saves the map when placement finishes and logs
   `OPEN_GOLF_CUT_TURF_ROUGH_COMPLETE`. Its backups are in `Saved/TurfAssetBackups/<time>-before-rough/`.
   - In DebugGame, placement blocks the editor for about 2.5 minutes.
   - If placement succeeds but the bake stops before saving, run the script again with `--finish`. It
     bakes and saves the grass already placed, without placing it again.
   - The bake's time limit counts editor frames (`OpenGolfCourse/Scripts/bake_clock.py`), so time
     stopped in a debugger does not use it up.

Done on 2026-11-20:
- The Blender source was backed up to `CutTurf-before-rough-20261120-220820.blend`.
- The previous exports were copied to `<art root>/CutTurf/ExportsBeforeRough/`.
- Rough was rebuilt and exported, and its textures baked. The 60 existing objects' vertex hashes and
  the 12 existing texture PNGs are unchanged.

Applied to the public map on 2026-11-20, with one DebugGame editor (under gdb):
- **Import and field.** `BuildCutTurf.py --refresh` imported 16 Rough meshes and 3 textures.
  - The field was rebaked from the map's current outlines (the par-four layout, five regions); the
    field files on this machine were from 2026-10-27.
  - `FixTurfTransitions.py` rebuilt the materials. `M_CutTurfRough`, `M_CutTurfBlades` and
    `M_CutTurfSurface` recompile without errors.
  - Pixel instructions: surface 961→1030, blades 926→1014, rough 1175→1281.
- **Placement.** 159,998 course-grass instances: Rough 38,192 (1 m 8,719; 50 cm 2,893;
  25 cm 5,457; 12.5 cm 21,123), Fairway 67,517, FirstCut 48,651, Collar 5,638. No Grass Medium and no
  boundary strip. The map was saved.
- **Captures.** `UOpenGolf.Visual.CoastalTerrain` passed before and after
  (`Saved/Automation/CoastalReview/BeforeRough-20261121-062341`, `AfterRough-20261121-065541`).
  - Stationary shimmer (`Saved/Automation/CutTurf/rough-{before,after}-stability.txt`): the close
    grass view's changed pixels fell from 0.219% to 0.046%. The tee view's rose from 0.010% to
    0.021%; the edge view and tree canopies are unchanged.
  - View 9 pan, lower 40%: mean second difference fell from 9.6 to 5.4/255, and pixels above
    16/255 from 13.4% to 2.7%.
- **Look.** The rough first rendered lighter and more saturated than the fairway, with the darker first cut
  between them. The colour retune below replaced that look.

### Colour retune — 2026-11-20

The owner asked for darker greens like two reference screenshots, and chose to move every surface toward them.
Rendered sRGB means of the screenshots, which are the targets:
- rough `323C12` (0.155 lightness), with near-black gaps between the blades;
- first cut `708036` and fairway `6D7E3C` (0.36);
- green about `77883F` (0.39).

Each family's tones moved by the hue, saturation and lightness difference between target and capture, over three
passes. One cause found and fixed: `FixTurfTransitions.py` now reloads `turf_palette` first; without that, an editor
that had already loaded the palette applied the old colours.

New Mid tones:

| Family | Mid |
| --- | --- |
| Rough | `435126` |
| FirstCut | `5F6F37` |
| Fairway | `627240` |
| Collar | `63763F` |
| Green | `607140` |
| Tee | `667843` |

Rendered result (`ColorMatch3-20261121-073211`; the green from a viewport capture beside the flag):

| Surface | Rendered | Target |
| --- | --- | --- |
| Rough | `364214`, 0.168 | 0.155 |
| First cut | 0.343 | 0.357 |
| Fairway | 0.357 | 0.365 |
| Green | 0.364 | about 0.39 |
| Collar | 0.354 | — |
| Tee | 0.375 | — |

Shimmer and flicker:
- Stationary shimmer improved again: tee view 0.002% changed, close view 0.027%.
- View 9 pan flicker is 3.6% of pixels above 16/255. That is up from 2.7% with the flatter rough, and
  still far below the 13.4% of the Poly Haven rough.

Owner acceptance is pending.

Known issue, not caused by the colours, fixed on 2026-11-23: beside the par-four green's collar, the rough tiles stop
short and the savanna ground showed desert-brown steps in close views. The savanna rough mask was stale and did not
reach the green complex; see [Rough mask follows the par-four hole](#rough-mask-follows-the-par-four-hole--2026-11-23).

Pending:
- Owner acceptance of the colour.
- An after fingerprint (the retained trees and understory; course-grass placement does not rebuild them).
- Frame timing.
- Removing the old generated course-grass folder. It was still loaded, so it is kept until a later
  apply after an editor restart.

## Fringe only around the green — 2026-11-23

The owner asked for the fairway to meet the rough directly, with a fringe only around the green, and
for that fringe to take the colour of the fairway's first cut: the collar rendered too light beside the green
(about `7B8D43` against the first cut's `727F35` in the owner's screenshot). The owner chose to leave the 1 m strip
between the fairway's end and the front fringe (`savanna_hole.FAIRWAY_GAP_M`) as rough, and to match colour only.
The first cut was never a physics region (its preset used rough physics outside the fairway outline), so terrain,
outlines, physics and the course field texture are unchanged.

- **Colour.** Collar takes the first cut's tones (Mid `5F6F37`, Dry `717837`, Lush `4D612E`), hue amount .25 and
  brightness variation .20 (`turf_palette.py`). The collar keeps its 20 mm blades and collar texture grain; blades
  take the field colour, so they follow.
- **Field.** `turf_field.py` no longer draws a first-cut layer. The fairway meets the rough at the 6 cm mowing edge;
  `FirstCutWidthM` and `FirstCutOuterEdgeM` are gone. The rough ground's turf relief coverage is now the fairway's
  edge and the tee. `field_color` deletes the retired first-cut nodes from older graphs.
- **Placement.** `DA_TurfFirstCutGrass` is no longer placed. `DA_TurfRoughGrass` has no fairway clearance, so the
  rough reaches 8 cm from the fairway, as it does at the fringe, sand and tee.
- **Kept.** The FirstCut Blender family, meshes, textures, `DA_TurfFirstCut*` and `MI_TurfFirstCut*` stay as a
  rollback. `M_CutTurfRough` still samples the FirstCut normal and roughness maps for its relief.

Apply with one editor, the public map open and Play stopped:

1. `FixTurfTransitions.py`; wait for `OPEN_GOLF_TURF_FIELD_COMPLETE`. It rebuilds the three turf materials and
   configures every instance.
2. `RemoveFirstCut.py`; wait for `OPEN_GOLF_FIRST_CUT_REMOVED`. It backs up the presets it changes to
   `Saved/TurfAssetBackups/<time>-before-no-first-cut/`, re-creates the rough preset, drops the first cut from the
   course grass and re-places it, then saves the map. Use `--finish` if the bake stopped before saving.

Applied to the public map on 2026-11-23 with one DebugGame editor (Linux, UE 5.8.4, Vulkan SM6):
- **Materials.** `FixTurfTransitions.py` rebuilt them without errors. Pixel instructions fell: surface 1030→972,
  blades 1014→956, rough 1281→1222.
- **Placement.** `RemoveFirstCut.py` saved the map. Course grass is 111,740 instances: Rough 38,585, Fairway 67,517,
  Collar 5,638 and no FirstCut (previously 48,651). The rough tiles cover about 10,810 m², up from 10,113 m²: the
  old band around the fairway. The backup is `Saved/TurfAssetBackups/20261123-214525-before-no-first-cut/`.
- **Look** (editor viewport captures in `Saved/Automation/CutTurf/NoFirstCut/`). The fairway now meets the rough at
  its mowing line with no band (`FairwayEdge-compare.png`). From about the owner's original view, the green fringe
  renders about `758339`, lightness 0.34. In the owner's screenshot the fringe was `7B8D43` and the first-cut band
  `727F35`–`798439`. The fringe stays a little lighter and yellower than the green (about `6F7D3F`), so the ring
  still shows.
- **Rendered Play run.** `UOpenGolf.Visual.CoastalTerrain` passed (`CoastalReview/NoFirstCut-20261123-215012`,
  1792 × 1087). The before run is `Cloud-FullAfter-20261121-175057`, which has the same turf except the first cut and
  collar colour. It came from a separate offscreen Development editor (1787 × 1081), so small differences can come
  from the editor process.
  - **Stationary views:** within noise. The lower 40% changed tee 0.002→0.001%, edge 0.003→0.001% and close grass
    0.024→0.037%. With grass off, the close view also rose (0.002→0.008%).
  - **View 9 pan, lower 40%:** pixels above 16/255 rose from 3.6% to 9.7%, and the mean second difference from 5.9 to
    7.7/255. The rise is in the rough. That view's lower frame is now 32% rough, up from 15%, because the rough fills
    the old first-cut band in the foreground. In motion, the 50 mm rough flickers more than the 30 mm first cut did.
    The fairway-only third did not get worse (0.9→0.7%). If this shows in play, lower the rough's
    `RoughDetailStrength` (2.0) or `BladeTipAmount` first.
  - **Not measured in Play:** no CoastalTerrain view shows the putting green, so stationary shimmer on the fringe
    close up is unverified in Play.
- **Terrain and vegetation.** `UOpenGolf.Range.CoastalTerrain` passed on the saved map.
- **Not caused by this change:** the desert-brown steps beside the par-four green's collar remained. They were fixed
  later that day (see the next section).
- **Pending:** owner acceptance.

## Rough mask follows the par-four hole — 2026-11-23

The owner reported brown zig-zag steps along the back of the green's fringe. The savanna ground's rough mask
(`T_SavannaRoughMask`, read as `RoughWeight` by `M_CutTurfRough`) was baked on 2026-10-21 from the range layout
before the par-4 hole, and `BuildParFourHole.py` never re-baked it. The mask was solid to about x = 355 m and zero past
367 m, so beside the back half of the green and around the greenside bunker the ground under the rough drew desert.
The square rough tiles stop at least 8 cm short of a curved edge. The removed edge strip used to hide those gaps, and
elsewhere the ground in them is rough-coloured. 385 of the 1,399 play-outline points fell short of solid rough: 142 on
the fringe, 111 on the green, all 112 on the greenside bunker and 20 on the fairway bunker's far side.

- **Fix.** `RefreshRoughMask.py` re-bakes the mask in place from the current outlines. It keeps the same 1024 × 512
  texels over the detail bounds (−252 to 504 m by −126 to 126 m), so `MaskBounds` is unchanged.
  - Before changing anything, it checks every referencing instance's bounds and the asset's generator tag, then backs
    up the texture and its source.
  - It fails unless every play outline comes out solid rough (`savanna_layout.mask_shortfall`, covered by
    `test_savanna_layout.py`).
  - `BuildParFourHole.py` now re-bakes the mask after laying out the hole.
- **Side effect.** The ground 12–16 m around the green complex and past the fairway bunker is now rough-coloured and
  fades to desert, like the rest of the hole. A strip inside the fairway lost coverage, but the fairway material does
  not sample the mask. Only `MI_TurfRough`, `MI_SavannaRough` and `MI_SavannaNative` reference it, and
  `MI_SavannaNative` ignores it (`NativeOnly`).

Applied to the public map on 2026-11-23 in the owner's DebugGame editor (Linux, UE 5.8.4, Vulkan SM6, one editor):
`OPEN_GOLF_ROUGH_MASK_REFRESHED`, with all 1,399 points solid rough. The backup is
`Saved/TurfAssetBackups/20261124-015938-before-rough-mask/`. Only the texture changed; the map was not modified.

- **Look** (editor viewport captures at 1792 × 1087, in `Saved/Automation/CutTurf/RoughMask/`):
  - The steps along the back of the fringe are gone (`compare-green-back.png`, `compare-green-side.png`).
  - So are the brown flecks between rough blades behind the green and the brown rim of the greenside bunker.
  - The fairway-bunker view (`compare-fairway-bunker.png`) is otherwise unchanged.
- **Slow pan** (back of the green, 13 frames, 0.25° yaw steps, lower 40%):
  - Pixels whose second difference exceeds 16/255 fell from 35.5% to 25.9%, and the mean second difference from 18.8
    to 13.0/255.
  - The heat map (`pan-heat.png`) loses the bright line along the stepped edge.
- **Stationary** (four frames per view, lower 40%, pixels changing by more than 16/255): inconclusive.
  - Back of the green 8.8% → 5.2%; fairway bunker 10.1% → 8.0%.
  - Beside the green 0.002% → 3.7%, but the before frames there were identical, which suggests the viewport did not
    redraw them.
  - Editor captures taken one at a time are not a controlled shimmer measurement. Clouds and tree shadows also
    differ between the sets; the mask changes only ground colour.
- **Not verified:** a rendered Play run (no CoastalTerrain view shows the green) and frame timing.
- **Pending:** owner acceptance.

## Agent entry points

This tracked workflow is the turf handoff. Machine-local agent instruction files and the
external Blender source may be absent in a fresh checkout. Restore Git LFS assets before
opening Unreal, and use the shared [testing guide](Testing.md) for validation.

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
| `ApplyCutTurfRough.py` | Replace the tall rough and edge strip on an applied range with the 50 mm rough |
| `SmoothCourseOutlines.py` | Smooth the play outlines, rebuild only the ground meshes, re-place course grass |
| `RefreshRoughMask.py` | Re-bake the savanna ground's rough mask from the current outlines (after a layout change) |
| `FixTurfTransitions.py` | Import the course field, build the field materials, configure every instance |
| `turf_field.py` / `turf_shading.py` | Shared field shader and per-family profiles / wiring and instance configuration |
| `turf_mask.py` | Bake `T_CourseSurfaceField` (signed distances to fairway, green, fringe, sand); requires NumPy |
| `../../OpenGolfCourse/Scripts/outline_smoothing.py` | Curves through authored corners; crossing check |
| `blender/test_turf_rules.py` | Footprints, repeatability, coverage, triangle budget and blade width |
| `test_turf_mask.py` | Signed distances, clamping, zero crossings, layer order and file format |

The source contains four mesh families: `Fairway`, `FirstCut`, `Collar` and `Rough`. The first
three each have four variants (`A`–`D`) of five footprints: 50, 25, 12.5 and 6.25 cm squares, plus
50 by 5 cm edge strips. `Rough` has four variants of 1 m, 50, 25 and 12.5 cm squares and no edge
strip (`turf_rules.FOOTPRINTS`). This gives 76 meshes. Objects share the same origin for export; isolate one object
when inspecting it. Looking at all 60 overlaid objects is not a valid density preview.

Change `SURFACES` in `turf_rules.py` for nominal height, blade width, clump spacing, blades per
clump, filler fraction and base colour. The `Cut Turf` Geometry Nodes modifier adjusts height
on existing source objects. Most shape and distribution controls are in the Python generator,
not exposed as Geometry Nodes sockets. Direct mesh edits are preserved by a normal export.
Changing generator settings requires rebuilding the source after backing up artist edits.

Vertex colour is named `BladeHeight` for compatibility. Its channels now carry:

- Red: normalized blade height. Only the rough blades' thatch-to-tip gradient reads it; there is
  no vertex motion.
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

`BakeCutTurf.bake()` records `texture-means.json`; the field needs it. The fairway channel still reaches
2.5 m, from when it covered the first-cut band. Do not use `UOpenGolfTerrainBuilder.BuildTerrain` to
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
ground within a couple of sRGB levels. Analyze with `Assets/Scripts/grass_stability.py --profile coastal`,
and compare second differences in the View 9 pan for motion flicker. Blender previews are not
Unreal shimmer or performance evidence.

Historical turf captures (2026-10-28) are `AfterTurfField-20261028-042858`, compared against `BeforeEdgeRepair-20261028-032124`,
under `Saved/Automation/CoastalReview`. That run did not establish rendering cost because
its frame probe was capped. Its `UOpenGolf.Range.RolloutSurfaces` failure came from flat-range
assumptions that sent the fixed shot into the saved coastal bunker; it was not evidence
for changing the physics to match that layout.

Record final results and limits in this document (`Docs/PhasesStatus.md` was removed in
`d94f672`), and update the asset roadmap when the status changes. Owner review is required before claiming visual
acceptance. See the [testing guide](Testing.md) for the shared validation workflow.

## Maintained turf relief and quieter palette — 2026-10-27

The palette described here was replaced on 2026-11-20 (see the [colour retune](#colour-retune--2026-11-20)); the
relief settings still apply.

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
