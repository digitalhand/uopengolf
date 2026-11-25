# Course vegetation: trees, grass and surface edges by hand

This guide adds trees, undergrowth and rough grass to a course built with the [Course Designer](course_designer.md),
and reshapes its fairway, bunker and green edges. Hunter Ranch is the worked example. Everything here is done in the
Unreal editor by the course author; the scripted vegetation tools used on the savanna range do not run on a course
built in ground squares yet (see [Limits](#limits)).

## Table of contents

- [How a tree plays](#how-a-tree-plays)
- [Where things live](#where-things-live)
- [1. Before you start](#1-before-you-start)
- [2. Shape the surfaces](#2-shape-the-surfaces)
- [3. Get a tree actor](#3-get-a-tree-actor)
- [4. Place the trees](#4-place-the-trees)
- [5. Add undergrowth beneath the trees](#5-add-undergrowth-beneath-the-trees)
- [6. Add grass and ground cover to the rough](#6-add-grass-and-ground-cover-to-the-rough)
- [7. Change the surface look](#7-change-the-surface-look)
- [8. Check it](#8-check-it)
- [Limits](#limits)

## How a tree plays

The visible tree has no ball collision. `SM_IslandTree02` (Hunter Ranch) and `SM_Jacaranda` (the savanna range) have
no simple collision shapes, so a ball passes straight through them. Dropping one into a map gives scenery only. A tree
that plays needs two hidden helpers at exactly the same transform:

| Part | Mesh (Hunter Ranch; savanna) | Collision | Tag | What the ball does |
| --- | --- | --- | --- | --- |
| Visible tree | `SM_IslandTree02`; `SM_Jacaranda` (Nanite) | `NoCollision` | none | nothing |
| Wood | `SM_IslandTree02Collision`; `SM_JacarandaCollision` (trunk and main limbs; collides on its triangles) | `BlockAll` | `OpenFairway.Obstacle` | bounces off, keeping 30 % of its speed; it never settles on a limb |
| Crown | `SM_IslandTree02Canopy`; `SM_JacarandaCanopy` (one convex hull) | `OverlapAll` | `OpenFairway.Canopy` | flies through, losing about 63 % of its speed and spin over 4 m of leaves, and turns slightly |

The ball's contact channel, `GolfBall`, is blocked by `BlockAll`, overlapped by `OverlapAll` and ignored by
`NoCollision` (`Config/DefaultEngine.ini`). Ground checks ignore anything tagged `OpenFairway.Obstacle`, so a ball
under a tree lies on the ground beneath it. The rules are in
`Plugins/OpenFairwayPhysics/Source/OpenFairwayPhysics/Public/OpenFairway/Obstacles.h`.

On the savanna range the vegetation builder adds both helpers for every tree. A tree placed by hand needs them in one
actor; Hunter Ranch's is ready (step 3).

## Where things live

| Place | Holds | Ships in |
| --- | --- | --- |
| `Plugins/OpenGolfCourse` | The shared course kit: surface presets, vegetation presets and builder, flag, wind | Base game |
| `Plugins/OpenGolfSavanna` | Shared open-licence art: jacaranda, Nature Essentials grass and undergrowth, cut turf | Base game |
| `Plugins/HunterRanch` | The course: map, ground squares, its own vegetation under `Content/Vegetation/` and the tree's exporter in `Scripts/` | The course pack |

A course pack contains only its own plugin (and engine content). It can use shared art only if the base game already
has it: the startup map uses the three jacaranda meshes and all 18 Nature Essentials meshes, so those are safe.
Other shared assets are safe only if a base map uses them. Anything else goes in the course plugin:
- tree actors and foliage types
- any species no base map uses, such as Hunter Ranch's Island Tree 02

Record its licence in `Plugins/HunterRanch/NOTICE.md` and in the [Asset licence roadmap](../AssetLicenseRoadmap.md).
Content under `Plugins/HunterRanch/Content/Vegetation/` appears in the editor as `/HunterRanch/Vegetation/`.

## 1. Before you start

- Run one editor. Save the map and back up `Plugins/HunterRanch/Content/` (or commit it) before each session.
- Work in this order: surfaces first, then trees, then ground cover. A full course rebuild (step 2) makes a new
  `Course.umap` and `DA_Ground`.
- Keep everything you place in its own level, so a rebuild cannot remove it:
  1. Open `/HunterRanch/Course/Course`.
  2. In **Window → Levels**, choose **Levels → Create New…** and save it as `/HunterRanch/Vegetation/Vegetation`. It
     must sit outside `/HunterRanch/Course`, which a rebuild needs empty.
  3. Set its streaming method to **Always Loaded** and double-click it to make it the current level.
  4. Place all trees, undergrowth and painted grass while it is current. The Outliner shows which level each actor is in.
- After a full rebuild, open the new `Course` map and choose **Levels → Add Existing…**. Pick
  `/HunterRanch/Vegetation/Vegetation`, set it to **Always Loaded** and save the map.

Course Designer edits rebuild only the ground squares they touch and leave other actors alone, so they need no
re-adding.

## 2. Shape the surfaces

Rough is not drawn: it is whatever ground no fairway, tee, green or bunker covers. To shape the rough, shape the edges
of the surfaces around it. Water and the course boundary come from the map data and cannot be corrected yet.

**Small moves: the Course Designer.** Open **Tools → Golf → Course Designer**, enter
`/HunterRanch/Course/DA_HunterRanch` and choose **Open course**. Then:
1. Drag outline vertices; only the touched 64 m squares rebuild.
2. **Save**.
3. **Save corrections**, so the next build keeps the change. The designer moves existing vertices only; it cannot
   add or remove them.

**New or reworked shapes: `corrections.json`.** Edit `Assets/Data/CourseDesign/HunterRanch/corrections.json`.
Each region is keyed by its map feature ID. The outlines below are shortened; a real one needs dense vertices:

```json
"regions": {
  "way/123456": {"surface": "bunker", "outline_m": [[412.0, -210.0], [420.0, -204.0], [426.0, -212.0], [418.0, -220.0]]},
  "way/654321": null,
  "author/h07-left-bunker": {"surface": "bunker", "outline_m": [[520.0, -95.0], [527.0, -90.0], [531.0, -98.0]]}
}
```

- A known ID replaces that outline, `null` removes the feature, and a new ID (such as `author/...`) adds one.
  Surfaces are `fairway`, `tee`, `green` and `bunker`.
- Coordinates are local metres east and north of the course origin. The terrain area sits at the world origin, so for
  an Unreal location in centimetres, east = X / 100 and north = −Y / 100.
- After **Save corrections**, the file holds the full outline of every region you moved. Copy one as a starting point.
- Your outline is built exactly as written. Map-data outlines are smoothed when the site is prepared; yours are not.
  Every vertex becomes a fixed ground edge, so curves need vertices about 1 m apart, or 0.5 m on greens and
  bunkers. To turn a few corner points into a smooth curve through them, run:

  ```bash
  python3 -c 'import json, sys; sys.path.insert(0, "Plugins/OpenGolfCourse/Scripts"); from outline_smoothing import smooth_outline; print(json.dumps([[round(x, 2), round(y, 2)] for x, y in smooth_outline(json.load(sys.stdin), 0.5)]))' <<< '[[412, -210], [420, -204], [426, -212], [418, -220]]'
  ```

- Outlines must not cross. Keep a new shape apart from its neighbours, or wholly inside one. A bunker drawn inside a
  fairway is fine: new regions come last and a later region takes precedence.

Then rebuild, following steps 2–6 of the [Course Designer guide](course_designer.md#2-prepare-the-site):
1. Prepare the site.
2. Review it until `checks.json` is clean.
3. Back up `/HunterRanch/Course` to `Saved/CourseDesign/Backups/`.
4. Empty the folder and build.
5. Re-add the vegetation level (step 1).

Last, stand at the tee and look toward the green. Check a landmark on each side against the aerial.

## 3. Get a tree actor

**Hunter Ranch's tree is ready:** `/HunterRanch/Vegetation/Trees/BP_Tree_IslandTree02`. It is Poly Haven's
[Island Tree 02](https://polyhaven.com/a/island_tree_02) (CC0), standing in for the course's native blue oak until an
open-licence blue oak model exists:
- about 9 m tall at scale 1, with a crown about 11 m across, 0.92 million triangles
- leaves made smaller and denser than the source's so the crown fills like a blue oak's
- leaves tinted and desaturated toward the dull grey-green of the reference photo
  `<art root>/blue_oak/reference_images/Quercus_douglasii_mature_specimen_Mount_Diablo_6.jpg`; the bark keeps its
  scanned colour

A sample stands near hole 1 (`BP_Tree_IslandTree02_Hole1`, Outliner folder `Hole_1`).

Every tree actor has these three static mesh components under a static scene root, all at location 0, rotation 0 and
scale 1 (Hunter Ranch's meshes are in `/HunterRanch/Vegetation/Trees/IslandTree02/Meshes/`):

| Component | Static Mesh | Collision preset | Rendering | Other |
| --- | --- | --- | --- | --- |
| `Tree` | `SM_IslandTree02` | `NoCollision` | Visible, Cast Shadow on, Evaluate World Position Offset on, World Position Offset Disable Distance 15000 | |
| `Wood` | `SM_IslandTree02Collision` | `BlockAll` | Visible off, Hidden in Game on, Cast Shadow off | Component tag `OpenFairway.Obstacle` |
| `Canopy` | `SM_IslandTree02Canopy` | `OverlapAll` | Visible off, Hidden in Game on, Cast Shadow off | Component tag `OpenFairway.Canopy`; Generate Overlap Events off |

Every component is Static, with Can Ever Affect Navigation off. The tags are on the components, never on the actor:
an actor tag applies to all of its components, so the crown would become an obstacle. The tree's materials sway in
the wind; beyond the 150 m disable distance trees stay still and keep their shadows cached, as on the savanna.

**Change the Hunter Ranch tree.** Its project, `<art root>/IslandTree02/IslandTree02.blend`, stays outside the
repository.
1. Fetch the source once: `python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py island_tree_02`.
2. Set the size and leaves in `Plugins/HunterRanch/Scripts/ExportIslandTree.py` (`HEIGHT_M`, `LEAF_SIZE`,
   `LEAF_DENSITY`), or edit the geometry-node modifier in the project. Then export:

   ```bash
   blender --background --python Plugins/HunterRanch/Scripts/ExportIslandTree.py -- [--regenerate]
   ```

   `--regenerate` rebuilds the project from the source and discards your edits in it. The export computes the
   leaves' `Tint` and `Saturation` from the reference colour.
3. Look before importing:

   ```bash
   blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/PreviewTree.py -- \
     Saved/AssetImport/HunterRanch/IslandTree02/export.json Saved/Automation/IslandTree02
   ```

4. In the editor's Output Log (Cmd), import into the course plugin:
   `py "<project>/Plugins/OpenGolfSavanna/Scripts/ImportSavannaAssets.py" "<project>/Saved/AssetImport/HunterRanch/IslandTree02/export.json"`.
   Add `--materials-only` after a colour-only change. Placed trees update.
5. To try colours by hand, change `Tint`, `Saturation` and `Brightness` on `MI_IslandTree02Leaves`. A re-import
   resets them to the export's values, so put lasting values in `ExportIslandTree.py`.

**Another species.** Prepare the same three meshes in Blender, outside the repository under the Blender art root:
- the visible tree, with Nanite on
- a wood-only mesh of the trunk and main limbs, imported with collision on its triangles (complex as simple) and
  Nanite off
- one convex hull around the crown (`UCX_` in the FBX), with Nanite off

`ImportSavannaAssets.py` documents these import settings; `ExportIslandTree.py` and
`Plugins/OpenGolfSavanna/Scripts/blender/tree_export.py` show how to export them. Never let leaf cards or twigs
block the ball. Then build its actor:

```bash
py "<project>/Plugins/OpenGolfCourse/Scripts/BuildTreeBlueprint.py" --tree <Tree mesh> --wood <Wood mesh> \
  --canopy <Canopy mesh> --blueprint /HunterRanch/Vegetation/Trees/BP_Tree_<Name>
```

It applies the table above (`tree_blueprint_rules.py`) and refuses to change a Blueprint it did not make. Building
the same actor by hand from the table also works.

## 4. Place the trees

1. **Landmark trees first.** From the aerial and the club's photos, place the trees that shape each hole: corner
   trees, trees guarding greens, and trees players aim over. Drag `BP_Tree_IslandTree02` in, press **End** to drop it
   to the ground, and set a random yaw. On a slope, lower it 10–30 cm so the downhill side of the trunk does not
   float.
2. **Size from the aerial.** At scale 1 the island tree is about 9 m tall with a crown about 11 m across, so set
   the scale to roughly crown width (m) ÷ 11. Keep it within 0.8–1.3 (about 7–12 m tall, mature blue-oak size):
   its wind motion is sized for that range. Use a uniform scale, so the wood and crown stay matched. (The
   jacaranda is 19.6 m tall with a 22 m crown at scale 1; the savanna uses 0.30–0.62.)
3. **Fill groups.** Alt-drag to copy, then vary the scale and yaw. The savanna's spacing is a good starting point
   (island tree scale = height ÷ 9):

   | Group | Distance from play | Spacing | Height |
   | --- | --- | --- | --- |
   | Near play | 8–25 m | about 12 m, at least 9 m apart | 7–8 m |
   | Outer | 25 m and beyond | about 27 m, at least 16 m apart | 7–11 m |
   | Backdrop | 45 m and beyond | about 42 m, at least 24 m apart | 8–12 m |

   Keep trees at least 3 m from fairway, green, tee and bunker edges, and off slopes steeper than 30°.
4. **Check sightlines** from every tee set. A tree that blocks a reasonable line to the fairway should be there on
   the real course.

## 5. Add undergrowth beneath the trees

The savanna places Nature Essentials pieces in rings under each crown. Its layout is in
`Plugins/OpenGolfSavanna/Scripts/understory_layout.py`. Rings are shares of the crown radius. The counts are for a
5.8 m crown radius, close to the island tree's at scale 1 (about 5.5 m):

| Category | Meshes (`/OpenGolfSavanna/Meshes/NatureEssentials/`) | Where | Pieces under a 5.8 m crown | Cull |
| --- | --- | --- | --- | --- |
| Litter | `SM_NE_Litter_A`–`D` | centre to 0.85, packed inward | 32 | 45 m |
| Moss | `SM_NE_MossDead_A`–`D` | 0.04–0.35, packed inward | 6 | 40 m |
| Seedlings | `SM_NE_Seedling_A`–`C` | 0.3–1.0 | 2 | 30 m |
| Rocks | `SM_NE_RockDark_A`/`B`, `SM_NE_RockLight_A`/`B` (scale 0.1–0.25) | 0.15–1.1 | 1.2 | 250 m |
| Shade grass | `SM_NE_GrassSmall`, `SM_NE_GrassClump` in tufts of 2 | 0.35–0.85 | 8 | 120 m |
| Drip-line grass | `SM_NE_GrassClump`, `SM_NE_GrassSeedy` in tufts of 3 | 0.8–1.35 | 36 | 120 m |

Counts grow with crown area: a crown 0.7 times as wide gets about half as many. Paint them with Foliage Mode, as in step 6, using one
foliage type per category. Keep them at least 2 m from play edges.

## 6. Add grass and ground cover to the rough

Use Foliage Mode with foliage types saved in the course plugin.

1. Make the vegetation level current. Open **Foliage Mode** and drag `SM_NE_GrassClump`, `SM_NE_GrassSeedy` and
   `SM_NE_GrassSmall` into the palette.
2. Save each new foliage type as `/HunterRanch/Vegetation/Foliage/FT_<name>`, for example `FT_RoughGrassClump`.
3. Set each foliage type:
   - **Instance settings:** Collision preset `NoCollision`, Cast Shadow off, Cull Distance 9000–12000.
     Evaluate World Position Offset on, with World Position Offset Disable Distance 6000.
   - **Placement:** Align to Normal on, Random Yaw on, Ground Slope Angle up to 30, Z Offset −2 to 0.
   - **Painting:** start at a low density (about 10 per 1000 × 1000 unit square, which is 100 m²) and raise it
     only after looking from the tee.
4. In the brush options, paint on **Static Meshes** only. Paint in the rough. Shift-paint erases anything that lands
   on a fairway, tee, green or bunker; keep about 2 m clear of their edges, as the savanna does.
5. Keep putting surfaces completely clear, so the ball stays visible.
6. After painting, check in the Outliner that the `InstancedFoliageActor` holding the grass belongs to the
   vegetation level.

Grass shading follows the project's stability rules (the shimmer checks in `AGENTS.md` and
[Rendered and hardware checks](../Testing.md#rendered-and-hardware-checks)):
- no grass shadows
- a palette close to the turf below
- wind motion only from the materials that already have it

The savanna's cut-turf carpet (mown 1 m tiles over fairway and rough) needs the scripted course-grass tool, which
does not run on this course yet.

## 7. Change the surface look

The ground squares use the course kit's coastal presets (`/OpenGolfCourse/Terrain/Presets/DA_Coastal*`). They set
each surface's material and the physical material that decides how the ball bounces and rolls.

`Plugins/OpenGolfCourse/Scripts/ApplyCourseGrass.py --surface-preset PRESET` swaps the material of every ground
mesh of that surface and updates the region presets, so later Course Designer edits keep it. It backs up the map and
terrain first. It has not been run on Hunter Ranch yet. A full rebuild sets the coastal presets again.

## 8. Check it

1. **Play.** Use the Course Designer's **Play hole**, then hit shots at a trunk and through a crown. The ball
   should bounce back off the wood and lose pace in the leaves without stopping dead. A ball under a tree should lie
   on the rough.
2. **Course tests.** These check tees, cups, a scored round and the lake penalty:

   ```bash
   bash Assets/Scripts/ue.sh test --engine-root <engine> --filter UOpenGolf.Course.HunterRanch
   ```

3. **Rendered capture.** Run `UOpenGolf.Visual.HunterRanch` with rendering, from the editor's automation window with
   one editor open; `ue.sh test` runs without rendering. It writes 20 frames of each view to
   `Saved/Automation/HunterReview/`: the tee, approach and green of holes 1, 9 and 12, plus a hole 1 flyover.
   - Capture once before you add vegetation and again after, at the same resolution and settings.
   - Compare the frames for shimmer in the crowns and grass, and pan slowly across a few holes in Play.
4. **Frame time.** Record frame time at the same views before and after. Only measure with one editor running.
5. **Pack.** Package with a new version and run `check_course_install.py`, as in
   [Course Designer step 7](course_designer.md#7-package-and-publish). This is the proof that the shared art arrived
   with the course; a missing asset shows as a grey or absent tree.
6. **Record** the results, with date, platform and limits, in [Phase status](../PhasesStatus.md).

## Limits

Recorded 2026-11-24.

- **Scripted vegetation does not run on a course built in ground squares.** The vegetation builder
  (`StartVegetation`, `RebuildVegetation`, `ApplyCourseGrass.py --grass-preset`) refuses Hunter Ranch for two reasons:
  - its terrain area has no generation ID, which only the savanna's single-area build sets
  - the full terrain check counts a 2,000,000-sample Landscape budget that Hunter Ranch's bounds exceed
    (1,575 × 1,386 m at 1 m, 2,182,950 samples), although this course has no Landscape

  Each preset or area is also capped at 100,000 instances. Hunter Ranch's fairways cover about 14 ha, so a course-wide
  1 m turf layer would exceed it.
- The Course Designer moves existing vertices only. Adding or removing vertices or features goes through
  `corrections.json` and a full rebuild.
- Author outlines are not smoothed or cleaned. Water, rough and the course boundary cannot be corrected.
- The in-game 3D grass switch (`uopengolf.Grass3D`) hides only builder-placed ground cover. Grass painted by hand
  stays visible.
- No automated test covers hand-built tree actors yet. The hole 1 tree's ball contact has not been tried in Play,
  and its wind motion has not been compared with the savanna's baked trees.
- The hole 1 sample tree sits in `Course.umap`, not the vegetation level. Once that level exists, select the tree,
  right-click the level in **Window → Levels** and choose **Move Selected Actors to Level**, or a full rebuild
  removes it.
- Loading the separate vegetation level from a downloaded course pack has not been checked yet; the pack check in
  step 8 covers it.
- `Assets/Scripts/grass_stability.py` reads the range and savanna capture names (`*-View<N>-Frame<NN>.png`), not the
  Hunter Ranch names, and the Hunter Ranch capture has no grass-off views. Shimmer on this course is judged by eye.
- Hunter Ranch's tree is a stand-in. Island Tree 02 is a scanned coastal tree with elliptic leaves and a leaning,
  twin trunk. No open-licence blue oak model was found on Poly Haven or Sketchfab (checked 2026-11-24). Its crown
  colour was measured in an editor capture against the reference photo: hue close, slightly warmer under the
  course's sun. Its leaves are about 2.4 times the source leaves' size, which reads as foliage from play distance.
