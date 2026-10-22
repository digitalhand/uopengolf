# Open Golf Savanna asset notices

The tree assets in `Content/Trees/Jacaranda/` come from
[Jacaranda Tree by Poly Haven](https://polyhaven.com/a/jacaranda_tree).
They retain [CC0 1.0 Universal](https://polyhaven.com/license/); the full text is in
[LICENSES/CC0-1.0.txt](LICENSES/CC0-1.0.txt). Project scripts and original material
graphs are covered by the repository's MIT licence.

The source files are pinned by size and MD5 in `Scripts/FetchPolyHaven.py`.
The fetcher also records SHA-256 checksums beside the downloaded sources.
`Scripts/blender/ExportJacaranda.py` prepares a trunk-base pivot, reduces geometry,
realises leaves and exports colour and normal textures at 2K and masks at 1K.
Unreal materials preserve the bark's vertex-colour blend and leaf transparency.
The hidden collision copy uses only trunk and branch sections, with Nanite disabled;
visible trees retain Nanite. Trees are static in this milestone.

Blender projects and downloaded originals are kept outside this repository.

## Grass (Poly Haven)

`Content/Meshes/Grass/Medium/`, `Content/Materials/Grass/Medium/` and `Content/Textures/Grass/Medium/` come from
[Grass Medium 01 by Poly Haven](https://polyhaven.com/a/grass_medium_01); `Content/Textures/Ground/LeafyGrass/`
comes from [Leafy Grass by Poly Haven](https://polyhaven.com/a/leafy_grass). Both are CC0 1.0 Universal
(`LICENSES/CC0-1.0.txt`) and pinned by size and MD5 in `Scripts/FetchPolyHaven.py`.

Changes: `Scripts/blender/ExportPolyHavenGrass.py` joins Grass Medium's LOD1 tufts into seeded square tiles
(1 m, 0.5 m and 0.25 m) and 1 x 0.25 m edge strips of shorter tufts, one tuft per 1/16 m cell. Colour maps are
premultiplied by their alpha and every map is written at 8 bits, 2K for colour and normals and 1K for masks.
Leafy Grass supplies only the textures for the ground under the tiles.

## Surface textures (ambientCG)

`Content/Textures/Surfaces/<asset>/` holds the 2K JPG colour, DirectX normal and roughness maps of:

| Asset | Use | Archive SHA-256 (`<asset>_2K-JPG.zip`) |
| --- | --- | --- |
| [Grass008](https://ambientcg.com/view?id=Grass008) | Fairway and green turf | `e83f37c63c1f5e3e4ee7d66cdb6b32277108c94e19cab7754595c5c60ed750e7` |
| [Ground093C](https://ambientcg.com/view?id=Ground093C) | Bunker sand | `7f0223582641a1407ff6050029ed46ad3c4e66f21a15ffb51880d092617f9ecf` |

Both are released under CC0 1.0 Universal (`LICENSES/CC0-1.0.txt`). `Scripts/FetchAmbientCG.py` downloads and
verifies them. The shared `M_SavannaSurface` material scales each texture's colour to a target colour, adds
broad variation and mowing stripes, and reduces normal detail at distance; no displacement, other maps or
Blender project is distributed.

## Desert ground (Poly Haven, ambientCG)

`Content/Textures/Ground/RedSand/` holds the 2K JPG colour, normal and roughness maps of
[Red Sand by Poly Haven](https://polyhaven.com/a/red_sand), pinned by size and MD5 in `Scripts/FetchPolyHaven.py`.
`Content/Textures/Ground/Rock029/` holds the 2K JPG colour and DirectX normal maps of
[Rock029 by ambientCG](https://ambientcg.com/view?id=Rock029), archive SHA-256
`88ae30780d19bda3577503df76fda3e2e1c01076f1e76a1d76d6dcd630d48fe2`, verified by `Scripts/FetchAmbientCG.py`.
Both are CC0 1.0 Universal (`LICENSES/CC0-1.0.txt`). `Materials/M_SavannaGround` (`Scripts/BuildSavannaGround.py`)
scales both to target colours, places Rock029 in noise-shaped patches over the sand, and blends in the Leafy
Grass rough near play through a per-course mask.

## Understory (Nature Essentials, CC BY 4.0)

The content in `Content/Meshes/NatureEssentials/`, `Content/Materials/NatureEssentials/` and
`Content/Textures/NatureEssentials/` is adapted from **"Realistic Nature Essentials" by CG Geek, made with
Graswald** ([product page](https://superhivemarket.com/products/nature-essentials-asset-pack)), used under the
[Creative Commons Attribution 4.0 International licence](https://creativecommons.org/licenses/by/4.0/). The full
text is in [LICENSES/CC-BY-4.0.txt](LICENSES/CC-BY-4.0.txt). This content keeps CC BY 4.0; the repository's MIT
licence does not cover it. No warranties are given, as the licence states.

Changes from the original pack, made by `Scripts/blender/ExportNatureEssentials.py` and `ne_rules.py`:

- chosen variants of grass, dead moss, rocks and oak seedlings
- meshes decimated to fixed triangle budgets
- leaf-litter patches assembled from the pack's oak leaves, twigs and small branches
- materials rebuilt for Unreal and recoloured for a dry savanna (tints and saturation in
  `Scripts/ImportNatureEssentials.py`)
- textures reduced to 2K colour and normal maps and 1K masks

The pack's own `.blend`, the GScatter add-on (GPL code) and other Graswald library items are not
distributed. The editable Blender project stays outside the repository.
