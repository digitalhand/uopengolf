# Third-party notices

No third-party code or purchased content is bundled. The public range includes
the following redistributable CC0 assets, with import dates recorded below. They retain CC0
1.0 Universal rather than the project's MIT license; attribution is not required
by CC0 and is recorded here for provenance.

| Asset / author | Source and license | Imported location |
| --- | --- | --- |
| Grass 004 / Lennart Demes (ambientCG) | [Source](https://ambientcg.com/view?id=Grass004), [CC0 terms](https://docs.ambientcg.com/license/) | `Plugins/OpenGolfCourse/Content/Surfaces/T_Grass_*.uasset`; used by `M_Turf` |
| Golf ball / jpsweber | [Source, CC0](https://opengameart.org/content/golf-ball) | `Plugins/OpenGolfCourse/Content/Ball/SM_GolfBall.uasset`, converted by `Plugins/OpenGolfCourse/Scripts/PrepareCourseAssets.py` through an editable `SM_GolfBall.blend` kept outside the repository |
| Jacaranda Tree / Poly Haven (2026-10-20) | [Source](https://polyhaven.com/a/jacaranda_tree), [CC0 terms](https://polyhaven.com/license) | `Plugins/OpenGolfSavanna/Content/Trees/Jacaranda/`; [notice and changes](Plugins/OpenGolfSavanna/NOTICE.md), [full licence](Plugins/OpenGolfSavanna/LICENSES/CC0-1.0.txt) |
| Grass Medium 01 and Leafy Grass / Poly Haven (2026-10-21) | [Grass Medium 01](https://polyhaven.com/a/grass_medium_01), [Leafy Grass](https://polyhaven.com/a/leafy_grass), [CC0 terms](https://polyhaven.com/license) | `Plugins/OpenGolfSavanna/Content/{Meshes,Materials,Textures}/Grass/Medium/`, `.../Textures/Ground/LeafyGrass/`; [notice and changes](Plugins/OpenGolfSavanna/NOTICE.md#grass-poly-haven) |
| Grass008 and Ground093C / ambientCG (2026-10-21) | [Grass008](https://ambientcg.com/view?id=Grass008), [Ground093C](https://ambientcg.com/view?id=Ground093C), [CC0 terms](https://docs.ambientcg.com/license/) | `Plugins/OpenGolfSavanna/Content/Textures/Surfaces/`; fairway, green and bunker candidates, [notice](Plugins/OpenGolfSavanna/NOTICE.md#surface-textures-ambientcg) |
| Red Sand / Poly Haven and Rock029 / ambientCG (2026-10-21) | [Red Sand](https://polyhaven.com/a/red_sand), [Rock029](https://ambientcg.com/view?id=Rock029), CC0 ([Poly Haven](https://polyhaven.com/license), [ambientCG](https://docs.ambientcg.com/license/)) | `Plugins/OpenGolfSavanna/Content/Textures/Ground/{RedSand,Rock029}/`; desert ground, [notice](Plugins/OpenGolfSavanna/NOTICE.md#desert-ground-poly-haven-ambientcg) |
| "Realistic Nature Essentials" / CG Geek, made with Graswald (2026-10-21) | [Product page](https://superhivemarket.com/products/nature-essentials-asset-pack), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (attribution required; not covered by the MIT licence) | `Plugins/OpenGolfSavanna/Content/{Meshes,Materials,Textures}/NatureEssentials/`; understory grass, moss, rocks, seedlings and leaf litter, [credit, licence text and changes](Plugins/OpenGolfSavanna/NOTICE.md#understory-nature-essentials-cc-by-40) |
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
| [open-lm-connector](https://github.com/digitalhand/open-lm-connector) | Launch-monitor integration, initially Square Home | Square protocol, session and Windows Bluetooth rewritten in C++ (see [launch-monitor integration](Docs/LaunchMonitorIntegration.md)) |
| [ogolf-physics](https://github.com/digitalhand/ogolf-physics) | Golf ball physics | Core equations, surfaces, ball profile and headless shot loop rewritten in C++ (see [OpenFairway Physics](Plugins/OpenFairwayPhysics/README.md)) |
| [OpenFairway](https://github.com/digitalhand/openfairway) | Earlier C# physics | Superseded by ogolf-physics; not ported |
| [ogolf](https://github.com/digitalhand/ogolf) | Reference for greens, lies, and surface integration | Not yet ported |

`Data/ShotLibrary/` stores results from the
[FlightScope Trajectory Optimizer](https://trajectory.flightscope.com/) for the
maintainer's recorded launch-monitor shots, as comparison data for the physics (see its
[README](Data/ShotLibrary/README.md#provenance)). No FlightScope code or assets are included.

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
