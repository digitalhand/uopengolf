# Asset licence roadmap

This roadmap is for replacing the restricted art in the private graphics baseline with
redistributable equivalents. The aim is to release the desert-savanna range on the public
map without losing its look. It lists each restricted dependency with its open replacement,
the obligations that replacement carries, and the evidence needed before the swap counts as
done. Licence policy is in [Licensing](Licensing.md); imported third-party work is recorded in
[third-party notices](../THIRD_PARTY_NOTICES.md).

Owner decisions (2026-10-20):

- The whole range becomes releasable. The public default map, `/Game/UOpenGolf/Maps/UOpenGolf`,
  becomes the savanna range.
- The private range stays as the visual reference until every row below has parity sign-off.
  After that it is retired.
- Only assets Unreal needs live in this repository. Blender projects and full-resolution
  sources stay outside it, under the Blender art root (`UOPENGOLF_BLENDER_ROOT`, default
  `~/Code/3D/blender/uopengolf`).

## Inventory

Status values: **Planned**, **In progress**, **Parity approved** (the owner signed off),
**Replaced** (the public map uses the replacement and the restricted item is no longer
referenced), and **Deferred** (open already; an upgrade is scheduled for later).

| # | Restricted item (licence) | Use in the private baseline | Open replacement (licence) | Parity check | Status |
| --- | --- | --- | --- | --- | --- |
| 1 | Cork oak, DZ Sample Trees (Fab) | 296 trees in 36 seeded patches | [Jacaranda Tree](https://polyhaven.com/a/jacaranda_tree), Poly Haven ([CC0](https://polyhaven.com/license)), scaled to the oak's canopy | Tee, edge and close views; oak close-up poses; probe frame time | In progress |
| 2 | Hidden oak wood-collision proxy (derived from the Fab oak) | Ball collision with trunk and branches, leaves passable | Jacaranda proxy colliding only on its trunk and branch sections (CC0 derivative) | Ball-sized sweeps hit wood and pass through leaves and gaps; airborne contact shots | In progress |
| 3 | Sand colour and normal, MW Landscape Auto Material (Fab) | Desert outer ground and the dry part of the rough blend | Poly Haven [`red_sand`](https://polyhaven.com/a/red_sand) (CC0) | Edge and tee captures beside the baseline; owner picks | In progress: replaced on the private map 2026-10-21 by Poly Haven `red_sand` (CC0, owner's pick over `aerial_sand`, whose dunes repeated every 2.5 m) in the public `M_SavannaGround`; brightness matched to MW within about 3%. Owner parity sign-off pending |
| 4 | Dirt colour, MW Landscape Auto Material (Fab) | Desert ground variation | ambientCG [Rock029](https://ambientcg.com/view?id=Rock029) (CC0), as rock patches | As row 3 | In progress: Rock029 (ambientCG, CC0) patches over the sand, owner's choice 2026-10-21, in `M_SavannaGround`. Owner parity sign-off pending |
| 5 | Grass clumps A–D, MW Landscape Auto Material (Fab) | Three distance bands, 19,716 clumps | Nature Essentials grass clump, seedy grass and small grass (CC BY 4.0) | Same cameras; worst-band shimmer within run-to-run noise of the baseline | In progress: the MW clumps are gone. Poly Haven Grass Medium tiles cover the rough to 12 m, and Nature Essentials grass (public since 2026-10-21) sits under the trees. Shimmer and owner parity pending |
| 6 | Grass blade atlas, MW Landscape Auto Material (Fab) | Default texture of the private grass material | Nature Essentials bentgrass colour texture (CC BY 4.0), already used by the understory grass | Material references contain no restricted package | In progress: Nature Essentials grass now uses the public `M_SavannaUnderstoryGrass` (2026-10-21). Only the unused private `M_PrivateDesertGrass` and `SM_PrivateGrassA–D` still reference the atlas; they go when the private range retires |
| 7 | — (already open-licence) | Oak understory: litter, moss, tufts, rocks and seedlings from Nature Essentials | Moves unchanged to the public content plugin (CC BY 4.0) | Instance counts match; grass switch hides the understory | In progress: moved to `/OpenGolfSavanna` with CC BY 4.0 notices and licence text 2026-10-21 (18 meshes, 10 materials). The private map's 18,185 understory pieces use the public meshes. The understory scripts are public, and the public map carries the same 18,185 pieces (2026-10-21) |
| 8 | — (already public) | Fairway, green, fringe and tee turf: ambientCG Grass004 (CC0) | Keep for now. Later: tiles rendered from Nature Essentials bentgrass (CC BY 4.0) | Turf shimmer within the tracked limits; green stripe contrast stays zero | Grass008 applied 2026-10-21 to the private map's fairway and, lighter and finer, its green; owner review, shimmer sequences and Play checks pending. Fringe and tee remain deferred |
| 9 | — (already public) | Bunker sand: the course kit's coastal sand material | [Ground093C](https://ambientcg.com/view?id=Ground093C), ambientCG ([CC0](https://docs.ambientcg.com/license/)), keeping `PM_Sand` | Bunker edge and close views; bunker contact and plow tests unchanged | Applied to the private map 2026-10-21; owner review pending |

Nothing else in the private baseline is restricted: terrain shape, layout, lighting, kit code
and scripts are already public or engine content. Rocks planned for the rough rework use
Nature Essentials rocks, not MW stones.

## Licence obligations

- **Poly Haven and ambientCG (CC0).**
  - No attribution is required. Provenance is still recorded in the third-party notices.
  - Downloads are pinned by hash in the fetch scripts.
  - Poly Haven's live API asks its users to make clear the assets come from Poly Haven. The
    fetch script sends a unique User-Agent, and the notices give that credit.
- **Nature Essentials (CC BY 4.0).** Superhive lists the pack's licence as "Creative Commons",
  which it defines as Creative Commons Attribution 4.0 International for asset products.
  Redistribution, commercial and game use, and changes are allowed with attribution. Every
  public copy carries:
  - **Credit:** "Realistic Nature Essentials" by CG Geek, made with Graswald.
  - **Links:** the [product page](https://superhivemarket.com/products/nature-essentials-asset-pack)
    and the [licence](https://creativecommons.org/licenses/by/4.0/).
  - **Changes note:**
    - chosen variants
    - decimated meshes
    - litter patches assembled from leaves, twigs and branches
    - recoloured materials
    - textures reduced to 2K

  The full licence text sits next to the assets in the public content plugin. The content
  keeps CC BY 4.0; the repository's MIT licence does not cover it.
- **Not distributed.** The GScatter add-on (GPL code) and other Graswald library items are not
  distributed or used.

## Gates

1. **Licence confirmation.** Before the first push of Nature Essentials content, the owner
   confirms the licence on the order page. If it is not CC BY 4.0 (or more permissive), Nature
   Essentials stays private: rows 5–7 and the later turf tiles in row 8 need CC0 alternatives
   instead. The owner confirmed CC BY 4.0 on 2026-10-21.
2. **Clean references.** No public asset may reference `/DZSampleTrees/`,
   `/MWLandscapeAutoMaterial/` or `/UOpenGolfPrivateRange/`. Check the dependencies of the public
   plugin and map in the editor before committing.
3. **Notices.** Third-party notices, licence texts and this table change in the same commit as
   the imported content.
4. **Repository size.** Committed texture sources are 2K, because an imported texture asset
   keeps its full source image. Higher-resolution originals stay with the Blender projects.
5. **Clean history (blocks any public release).** Restricted packs were committed in the past.
   Deleting them from the working tree leaves them in Git history, where anyone can download them:

   | Folder | Files | Commits | Branches |
   | --- | ---: | --- | --- |
   | `Content/PN_GrassLibrary` (Project Nature, Fab) | 626 | added `59d074c`, deleted `c645f87` | `main` and every branch |
   | `Content/DZ_Assets` (DZ Sample Trees, Fab) | 177 | `4898a93` | `feature/improving-terrain-game-play` |
   | `Content/MWLandscapeAutoMaterial` (Fab) | 100 | `4898a93` | `feature/improving-terrain-game-play` |

   Owner decision (2026-10-21): the public release goes out as a new repository started from a
   clean tree, so none of this history is carried over. Done 2026-10-21: the owner re-initialised
   the repository without its history.
   Before that first push, check the clean tree with Gate 2 and confirm that none of the folders
   above, nor `Plugins/PrivateContent/`, is included.

## Parity method

A replacement is approved only when the owner signs off on evidence gathered the same way for
the private baseline and the open version:

- **Cameras.** `UOpenGolf.Visual.CoastalTerrain` cameras at the same resolution and AA, with
  grass on and off, plus fixed viewport poses 3–9 m from a tree trunk.
- **Stability.** `Scripts/grass_stability.py --profile coastal`. The lower crop must stay at or
  below about 0.05% changed pixels. Worst-band changes should stay within the run-to-run noise
  measured with grass hidden.
- **Frame time.** The probe camera with ground cover off and on (`RangeGraphicsProbe.py
  understory`). Report it as provisional unless the run was a controlled 1080p one.
- **Gameplay.** Saved-terrain contact, obstacle, bunker and rollout-surface tests pass on the
  public map. Physical materials are unchanged.

Results go in [Phase status](PhasesStatus.md), and the row's status changes here.

## Sequence

1. Move Blender projects out of the repository and make the export scripts resolve the art root.
2. Replace the oak with the jacaranda (rows 1–2) in the existing authored range, with map and asset backups first. The owner chose in-place conversion on 2026-10-20; do not create a scratch map. Retain oak captures and backups for comparison.
3. Move Nature Essentials into the public content plugin with its CC BY 4.0 notices (owner
   decision 2026-10-20), then replace the ground and grass (rows 3–6) with spikes compared
   against the baseline.
4. Convert the public map, moving the understory scripts into the public plugin (row 7).
   Done 2026-10-21 with `OpenGolfSavanna/Scripts/BuildSavannaRange.py` and `ApplyCourseGrass.py`
   (see [Phase status](PhasesStatus.md#savanna-range-on-the-public-map--2026-10-21)); range tests
   on the public map and parity sign-off remain.
5. Get parity sign-off for every row, then retire the private range. Turf (row 8) and the rough rework continue on the public map.
6. Later: trees sway with the range wind. Trees stay static until then; their wood collision
   stays static too.

## Jacaranda checkpoint — 2026-10-20

The in-place migration and regeneration scripts are prepared. The imported tree keeps
Nanite; its hidden wood proxy disables Nanite and enables triangle collision only on
trunk and branch sections. Native vegetation presets now support an optional collision
mesh at the visible tree transform. Development builds successfully.

The existing map still contains 296 oaks and 296 oak colliders. Applying the migration
requires rebuilding and restarting the running DebugGame editor to load the new field.
The owner rebuilt and restarted DebugGame on 2026-10-20 and the field loaded, but at
17:57 the presets and the saved map still used the oak. The migration had not run yet.
It was applied later that day; results are in [Phase status](PhasesStatus.md)
("Jacaranda applied in place"). Rows 1–2 stay In progress until owner review.
Before captures contain 200 frames; measurements and pending checks are in
[Phase status](PhasesStatus.md). No jacaranda gameplay, final shimmer or parity approval
is claimed at this checkpoint.

## Fairway and boundary checkpoint — 2026-10-21

The owner brought rough-edge gap filling and fairway texture work forward on the current
savanna range, with reusable tools and README instructions. Grass008 (ambientCG, CC0) is
now imported at 2K as a separate fairway candidate in OpenGolfSavanna. The putting green
is excluded. Boundary fill reuses the existing Poly Haven Grass Medium tile family;
this does not complete the earlier restricted-grass or understory migration rows.

Both saved maps remain unchanged pending confirmation of the latest grass placements.
See the rough boundary/fairway checkpoint in `PhasesStatus.md` for build/test evidence,
before-only shimmer measurements and remaining visual checks. No parity approval is implied.

## Course grass and surfaces applied — 2026-10-21

Superseding the checkpoint above, the private map now carries the open-licence course layer:

- **Rough:** Poly Haven Grass Medium (CC0) tiles cover the rough from the play outlines out to
  12 m, plus a row of shorter edge strips along the outlines. The rough ground fades to the
  desert from 12 to 16 m. Scattered tiles further out were tried and dropped at the owner's
  request.
- **Fairway and green:** Grass008 (ambientCG, CC0), row 8.
- **Bunkers:** Ground093C (ambientCG, CC0), row 9.

Rows 5 and 7 are unchanged: this layer does not replace the Nature Essentials grass and
understory around the trees. Results, backups and pending checks are in
[Phase status](PhasesStatus.md#course-grass-and-surfaces-applied--2026-10-21). No parity
approval is implied.
