# Third-party notices

The project bundles the MIT-licensed Project Nayuki QR encoder in
[OpenGolfOnline](Plugins/OpenGolfOnline/NOTICE.md). No purchased content is bundled. The public range and the
built-in player avatars include the following redistributable assets, with import dates recorded below.
The CC0 assets retain CC0 1.0 Universal rather than the project's MIT license; attribution is not
required by CC0 and is recorded here for provenance. Other rows name their own licence and obligations.

| Asset / author | Source and license | Imported location |
| --- | --- | --- |
| Grass 004 / Lennart Demes (ambientCG) | [Source](https://ambientcg.com/view?id=Grass004), [CC0 terms](https://docs.ambientcg.com/license/) | `Plugins/OpenGolfCourse/Content/Surfaces/T_Grass_*.uasset`; used by `M_Turf` |
| Golf ball / jpsweber | [Source, CC0](https://opengameart.org/content/golf-ball) | `Plugins/OpenGolfCourse/Content/Ball/SM_GolfBall.uasset`, converted by `Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py` through an editable `SM_GolfBall.blend` kept outside the repository |
| Jacaranda Tree / Poly Haven (2026-10-20) | [Source](https://polyhaven.com/a/jacaranda_tree), [CC0 terms](https://polyhaven.com/license) | `Plugins/OpenGolfSavanna/Content/Trees/Jacaranda/`; [notice and changes](Plugins/OpenGolfSavanna/NOTICE.md), [full licence](Plugins/OpenGolfSavanna/LICENSES/CC0-1.0.txt) |
| Grass Medium 01 and Leafy Grass / Poly Haven (2026-10-21) | [Grass Medium 01](https://polyhaven.com/a/grass_medium_01), [Leafy Grass](https://polyhaven.com/a/leafy_grass), [CC0 terms](https://polyhaven.com/license) | `Plugins/OpenGolfSavanna/Content/Grass/Medium/{Meshes,Materials,Textures}/`, `.../Ground/Textures/LeafyGrass/`; [notice and changes](Plugins/OpenGolfSavanna/NOTICE.md#grass-poly-haven) |
| Grass008 and Ground093C / ambientCG (2026-10-21) | [Grass008](https://ambientcg.com/view?id=Grass008), [Ground093C](https://ambientcg.com/view?id=Ground093C), [CC0 terms](https://docs.ambientcg.com/license/) | `Plugins/OpenGolfSavanna/Content/Surfaces/Textures/`; fairway, green and bunker candidates, [notice](Plugins/OpenGolfSavanna/NOTICE.md#surface-textures-ambientcg) |
| Red Sand / Poly Haven and Rock029 / ambientCG (2026-10-21) | [Red Sand](https://polyhaven.com/a/red_sand), [Rock029](https://ambientcg.com/view?id=Rock029), CC0 ([Poly Haven](https://polyhaven.com/license), [ambientCG](https://docs.ambientcg.com/license/)) | `Plugins/OpenGolfSavanna/Content/Ground/Textures/{RedSand,Rock029}/`; desert ground, [notice](Plugins/OpenGolfSavanna/NOTICE.md#desert-ground-poly-haven-ambientcg) |
| "Realistic Nature Essentials" / CG Geek, made with Graswald (2026-10-21) | [Product page](https://superhivemarket.com/products/nature-essentials-asset-pack), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (attribution required; not covered by the MIT licence) | `Plugins/OpenGolfSavanna/Content/NatureEssentials/{Meshes,Materials,Textures}/`; understory grass, moss, rocks, seedlings and leaf litter, [credit, licence text and changes](Plugins/OpenGolfSavanna/NOTICE.md#understory-nature-essentials-cc-by-40) |
| Fluent Emoji (3D) / Microsoft (2026-11-24) | [Source at `1ffb34c`](https://github.com/microsoft/fluentui-emoji/tree/1ffb34c752ecf5d402f04cfb4b392c77f57c54bc), MIT (licence text must accompany copies) | `Plugins/OpenGolfSavanna/Content/UI/Avatars/T_Avatar_*.uasset`; 24 built-in player avatars, [notice](Plugins/OpenGolfSavanna/NOTICE.md#player-avatars-fluent-emoji-mit), [full licence](Plugins/OpenGolfSavanna/LICENSES/MIT-fluentui-emoji.txt) |
| Hunter Ranch course layout / OpenStreetMap contributors (2026-11-24) | [OpenStreetMap](https://www.openstreetmap.org/copyright), [Open Database License 1.0](https://opendatacommons.org/licenses/odbl/1-0/) (attribution and share-alike for the derived layout; not covered by the MIT licence) | Course outlines, hole routes and lakes in the downloadable `Plugins/HunterRanch` content; author corrections in `Assets/Data/CourseDesign/HunterRanch/`, [notice](Plugins/HunterRanch/NOTICE.md). The derived layout is published under ODbL beside each course pack |
| Hunter Ranch elevation / USGS 3D Elevation Program (2026-11-24) | [1 m DEM tile](https://www.sciencebase.gov/catalog/item/60d16eecd34e86b938ac09cf), US public domain | Ground heights of the downloadable `Plugins/HunterRanch` course; the source tile is cached outside the repository |
| Island Tree 02 / Rob Tuytel and Rico Cilliers, Poly Haven (2026-11-24) | [Source](https://polyhaven.com/a/island_tree_02), [CC0 terms](https://polyhaven.com/license) | Hunter Ranch's tree in the downloadable `Plugins/HunterRanch/Content/Vegetation/Trees/IslandTree02/`; [notice and changes](Plugins/HunterRanch/NOTICE.md), [full licence](Plugins/HunterRanch/LICENSES/CC0-1.0.txt) |
| Golf Set flagstick / mess110 (historical) | [Source, CC0](https://opengameart.org/content/golf-set) | Earlier `SM_Flagstick.uasset`; replaced by original dimensioned geometry on 2026-10-15 |

The [CC0 1.0 legal text](https://creativecommons.org/publicdomain/zero/1.0/legalcode)
applies to these assets and their conversions. Archive SHA-256 hashes and exact
downloads are pinned in `Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py`; conversion and
import details are in the [OpenGolfCourse README](Plugins/OpenGolfCourse/README.md). Grass-blade
geometry, the course kit's material graphs and the current flagstick geometry are original MIT contributions. Other Golf Set models and
textures are not included.

The reference projects below are the maintainer's own (`digitalhand`) projects. Code
ported from them is rewritten in C++ and covered by this repository's MIT license;
no separate notice is required. Third-party dependencies or assets inside those
projects keep their own licenses and are recorded here if imported.

| Project | Intended use | Status |
| --- | --- | --- |
| [open-lm-connector](https://github.com/digitalhand/open-lm-connector) | Launch-monitor integration, initially Square Home | Square protocol, session and Windows Bluetooth rewritten in C++ (see [launch-monitor reference](Plugins/OpenLaunchMonitor/Docs/Reference.md)) |
| [ogolf-physics](https://github.com/digitalhand/ogolf-physics) | Golf ball physics | Impact, ground, rest, surface and cup-capture equations and the headless shot loop rewritten in C++; the flight is uopengolf's own fitted model (see [OpenFairway Physics](Plugins/OpenFairwayPhysics/README.md)) |
| [OpenFairway](https://github.com/digitalhand/openfairway) | Earlier C# physics | Superseded by ogolf-physics; not ported |
| [ogolf](https://github.com/digitalhand/ogolf) | Reference for greens, lies, and surface integration | Not yet ported |

`Assets/Data/ShotLibrary/` stores only uopengolf's own shot recordings. Third-party reference data
used to check and fit the physics (launch-monitor exports, published averages, calculator output)
and research papers are kept outside the repository and are not distributed with it (see
[Assets/Data](Assets/Data/README.md#where-the-data-lives)); the physics docs cite the papers.

The OpenGolfEnvironment cloud material references Epic’s `T_VolumeNoiseShape128` and
`T_VolumeNoiseErosion32` in the engine’s EngineSky content.

Unreal Engine must be obtained separately and is governed by the
[Unreal Engine EULA](https://www.unrealengine.com/eula/unreal), not this repository's MIT license.
The greeting uses Unreal's built-in font at runtime; no engine font or engine source is copied into this repository.

When adding code or assets from anyone else, record the source, revision/version,
applicable license, required notices, and repository location here. Put any
required full license texts next to the imported work or in a documented license directory.
Purchased assets and their derivatives must not enter public Git or Git LFS history
unless their license explicitly permits that redistribution.

See [licensing and asset policy](Docs/Licensing.md).

### ambientCG surface candidates

[Grass008](https://ambientcg.com/view?id=Grass008) (fairway and green) and
[Ground093C](https://ambientcg.com/view?id=Ground093C) (bunker sand), ambientCG, CC0 1.0 Universal.
The public OpenGolfSavanna plugin imports their 2K color, DirectX normal and roughness maps.
Its notice records the pinned download hashes and material changes. See
[OpenGolfSavanna notices](Plugins/OpenGolfSavanna/NOTICE.md#surface-textures-ambientcg).
