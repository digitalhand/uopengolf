# Third-party notices

No sibling-project code or purchased content is bundled in this foundation.
The following projects are planned references, not current build dependencies.

| Project | Intended use | Current licensing evidence |
| --- | --- | --- |
| [OpenFairway](https://github.com/digitalhand/openfairway) | C# physics reference for a later C++ port | MIT in the local repository and addon license files; preserve the original notices when importing |
| [open-lm-connector](https://github.com/digitalhand/open-lm-connector) | Launch-monitor integration, initially Square Home | MIT in the local `addons/LICENSE`; review all imported dependencies and preserve notices |
| ogolf, sibling repository | Reference for greens, lies, and surface integration | No root license found during initial inspection; verify permission before copying implementation or assets |

Unreal Engine must be obtained separately and is governed by the
[Unreal Engine EULA](https://www.unrealengine.com/eula/unreal), not this repository's MIT license.
The greeting uses Unreal's built-in font at runtime; no engine font or engine source is copied into this repository.

When adding third-party code or assets, record the source, revision/version,
applicable license, required notices, and repository location here. Put any
required full license texts next to the imported work or in a documented license directory.
Purchased assets and their derivatives must not enter public Git or Git LFS history
unless their license explicitly permits that redistribution.

See [licensing and asset policy](Docs/Licensing.md).
