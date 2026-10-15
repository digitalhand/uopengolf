# Third-party notices

No purchased content is bundled. Code ported from sibling projects is listed under
[Imported code](#imported-code); the table below lists references and their status.

| Project | Intended use | Current licensing evidence |
| --- | --- | --- |
| [OpenFairway](https://github.com/digitalhand/openfairway) | C# physics reference for a later C++ port | MIT in the local repository and addon license files; preserve the original notices when importing |
| [open-lm-connector](https://github.com/digitalhand/open-lm-connector) | Launch-monitor integration, initially Square Home | MIT in `addons/LICENSE`; ported in part, see below |
| ogolf, sibling repository | Reference for greens, lies, and surface integration | No root license found during initial inspection; verify permission before copying implementation or assets |

Unreal Engine must be obtained separately and is governed by the
[Unreal Engine EULA](https://www.unrealengine.com/eula/unreal), not this repository's MIT license.
The greeting uses Unreal's built-in font at runtime; no engine font or engine source is copied into this repository.

## Imported code

| Source | Revision | License | Destination | Notes |
| --- | --- | --- | --- | --- |
| [open-lm-connector](https://github.com/digitalhand/open-lm-connector) `addons/square/SquareProtocol.cs`, `SquareCommandBuilder.cs`, `SquareShotMetrics.cs`, `SquareConnectionOptions.cs`, `SquareConnectionSession.cs`; `addons/common/bluetooth/IBluetoothGattClient.cs`, `windows/WindowsBluetoothGattClient.cs` | `92eb021abe28d8eb1abe43f87767d3eb73e5939e` | MIT | `Plugins/OpenLaunchMonitor/Source/OpenLaunchMonitor/Private/` | Translated to C++; each derived file names its source. Test expectations and recorded frames come from its `tests/`. No dependencies were imported. |

open-lm-connector's license (`addons/LICENSE`):

```text
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

When adding third-party code or assets, record the source, revision/version,
applicable license, required notices, and repository location here. Put any
required full license texts next to the imported work or in a documented license directory.
Purchased assets and their derivatives must not enter public Git or Git LFS history
unless their license explicitly permits that redistribution.

See [licensing and asset policy](Docs/Licensing.md).
