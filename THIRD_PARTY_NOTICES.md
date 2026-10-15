# Third-party notices

No third-party code or purchased content is bundled. The public range includes
the following redistributable CC0 assets, imported on 2026-10-15. They retain CC0
1.0 Universal rather than the project's MIT license; attribution is not required
by CC0 and is recorded here for provenance.

| Asset / author | Source and license | Imported location |
| --- | --- | --- |
| Grass 004 / Lennart Demes (ambientCG) | [Source](https://ambientcg.com/view?id=Grass004), [CC0 terms](https://docs.ambientcg.com/license/) | `Content/UOpenGolf/Art/T_Grass_*.uasset`; used by `M_Fairway` |
| Golf ball / jpsweber | [Source, CC0](https://opengameart.org/content/golf-ball) | `Content/UOpenGolf/Art/SM_GolfBall.uasset` |
| Golf Set flagstick / mess110 | [Source, CC0](https://opengameart.org/content/golf-set) | `Content/UOpenGolf/Art/SM_Flagstick.uasset` |

The [CC0 1.0 legal text](https://creativecommons.org/publicdomain/zero/1.0/legalcode)
applies to these assets and their conversions. Archive SHA-256 hashes and exact
downloads are pinned in `Scripts/PrepareRangeAssets.py`; conversion and import
details are in [Range assets](Docs/RangeAssets.md). Grass-blade geometry and the
game's material graphs are original MIT contributions. Other Golf Set models and
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
