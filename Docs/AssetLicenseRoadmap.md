# Asset licence roadmap

This roadmap records the third-party art on the desert-savanna range, the licence each item
carries, and the checks it still needs. The range is the public map,
`/Game/UOpenGolf/Maps/UOpenGolf`, which is the project's only map. Licence policy is in
[Licensing](Licensing.md); imported third-party work is recorded in
[third-party notices](../THIRD_PARTY_NOTICES.md).

Owner decisions:

- 2026-10-20: the whole range is releasable, so every asset on it must be redistributable.
- 2026-10-28: the public map is the only map. Open review items below are public-map work.
- Only assets Unreal needs live in this repository. Blender projects and full-resolution
  sources stay outside it, under the Blender art root (`UOPENGOLF_BLENDER_ROOT`, default
  `~/Code/3D/blender/uopengolf`).

## Inventory

Status values: **In progress** (on the map, owner review or checks pending), **Approved** (the
owner signed off), and **Deferred** (an upgrade is scheduled for later).

| # | Use on the range | Asset (licence) | Review check | Status |
| --- | --- | --- | --- | --- |
| 1 | 296 trees in 36 seeded patches | [Jacaranda Tree](https://polyhaven.com/a/jacaranda_tree), Poly Haven ([CC0](https://polyhaven.com/license)) | Tee, edge and close views; tree close-up poses; probe frame time | In progress: distant canopies and leaf tint need owner review |
| 2 | Ball collision with trees | Jacaranda trunk-and-main-limb collider exported from Blender, plus a convex canopy volume that slows the ball (CC0 derivatives) | Ball-sized sweeps hit wood and pass through branches, leaves and gaps; airborne contact shots | In progress: 296 trunk colliders and 296 canopy volumes (296/296 hits, 888 clear; trunk rebounds and canopy slows in Play, 2026-10-21); owner review pending |
| 3 | Desert outer ground and the dry part of the rough blend | Poly Haven [`red_sand`](https://polyhaven.com/a/red_sand) (CC0) in `M_SavannaGround` | Edge and tee captures; owner review | In progress: owner's pick over `aerial_sand`, whose dunes repeated every 2.5 m (2026-10-21) |
| 4 | Desert ground variation | ambientCG [Rock029](https://ambientcg.com/view?id=Rock029) (CC0), as rock patches in `M_SavannaGround` | As row 3 | In progress: owner's choice 2026-10-21 |
| 5 | Rough grass | Poly Haven Grass Medium tiles (CC0) to 12 m from the play outlines; Nature Essentials grass clump, seedy grass and small grass (CC BY 4.0) under the trees | Same cameras; worst-band shimmer within run-to-run noise | In progress: shimmer and owner review pending |
| 6 | Understory grass material | Nature Essentials bentgrass colour texture (CC BY 4.0) in `M_SavannaUnderstoryGrass` | Material references contain only project, engine or open-licence packages | In progress since 2026-10-21 |
| 7 | Tree understory: litter, moss, tufts, rocks and seedlings | Nature Essentials (CC BY 4.0) in `/OpenGolfSavanna`, 18 meshes and 10 materials | Instance counts match; grass switch hides the understory | In progress: 18,185 pieces on the public map (2026-10-21) |
| 8 | Fairway, first cut, collar, green and tee turf | Original project cut-turf art (MIT); Grass008, ambientCG (CC0), as the earlier fairway and green texture | Turf shimmer within the tracked limits; green stripe contrast stays zero | In progress: the denser original turf was applied 2026-10-27; rendered validation and owner acceptance pending |
| 9 | Bunker sand | [Ground093C](https://ambientcg.com/view?id=Ground093C), ambientCG ([CC0](https://docs.ambientcg.com/license/)), keeping `PM_Sand` | Bunker edge and close views; bunker contact and plow tests unchanged | In progress: owner review pending |

Terrain shape, layout, lighting, kit code and scripts are project work or engine content. Rocks
planned for the rough rework use Nature Essentials rocks.

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
   confirms the licence on the order page. If it is not CC BY 4.0 (or more permissive), rows 5–7
   need CC0 alternatives instead. The owner confirmed CC BY 4.0 on 2026-10-21.
2. **Clean references.** Every package the public map and plugins depend on is project, engine or
   open-licence content listed here. Check the dependencies of the public plugins and map in the
   editor before committing.
3. **Notices.** Third-party notices, licence texts and this table change in the same commit as
   the imported content.
4. **Repository size.** Committed texture sources are 2K, because an imported texture asset
   keeps its full source image. Higher-resolution originals stay with the Blender projects.

## Review method

An item is approved only when the owner signs off on evidence gathered the same way before and
after a change:

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

## Next steps

1. Owner review of every row, using the review method above.
2. Turf (row 8) and the rough rework continue on the public map.
3. Later: trees sway with the range wind. Trees stay static until then; their wood collision
   stays static too.

## History

- **2026-10-20:** Blender projects moved out of the repository; the export scripts resolve the
  art root. The jacaranda (rows 1–2) was prepared with a hidden wood proxy that keeps the visible
  tree on Nanite. See [Phase status](PhasesStatus.md) ("Jacaranda applied in place").
- **2026-10-21:** Grass008 (fairway, green) and Ground093C (bunkers) applied, with Poly Haven
  Grass Medium tiles covering the rough to 12 m. `red_sand` and Rock029 replaced the desert
  ground, and Nature Essentials moved into `/OpenGolfSavanna` with its CC BY 4.0 notices. The
  public map was built with `OpenGolfSavanna/Scripts/BuildSavannaRange.py` and `ApplyCourseGrass.py`
  (see [Phase status](PhasesStatus.md#savanna-range-on-the-public-map--2026-10-21)).
- **2026-10-27:** the owner approved an original blade family and matching baked textures for
  fairway, first cut, green collar and putting green. The first pass was rejected as sparse; the
  denser revision was baked at 2K, refreshed in the public plugin and refined (first-cut relief,
  filtered short-turf normals, quieter olive variation). It introduces no third-party assets. See
  [Blender grass handoff](TurfWorkflow.md).
- **2026-10-28:** the public map became the only map.
