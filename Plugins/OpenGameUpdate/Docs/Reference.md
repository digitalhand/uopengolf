# OpenGameUpdate reference

### Configuration

All project-specific values are in `[OpenGameUpdate]` in `Config/DefaultGame.ini`:

```ini
[OpenGameUpdate]
Repository=owner/name
InstallerAsset=MyGame-{version}-Windows-x64-Setup.exe
InstallerArguments=/SILENT /SUPPRESSMSGBOXES /NORESTART /DIR="{installdir}"
```

| Key | Meaning |
| --- | --- |
| `Repository` | GitHub `owner/name`. Its latest published release is the update; drafts and prereleases are never offered |
| `InstallerAsset` | The installer's asset name. `{version}` is the release tag without its leading `v` |
| `InstallerArguments` | The installer's command line. `{installdir}` is the running game's folder, without a trailing slash |

The current version is the project's `ProjectVersion`
(`[/Script/EngineSettings.GeneralProjectSettings]`). The plugin compares versions with
Semantic Versioning: `v1.2.3`, with an optional `-prerelease`. A release is newer than its own prerelease and its
`-dev` builds. Versions that cannot be read never prompt an update.

The release must also have `<InstallerAsset>.sha256`, one `<hash>  <name>` line as written by `sha256sum`. Without
it, the update is still reported but cannot be installed from the game.

### Quick start

```cpp
#include "GameUpdateSubsystem.h"

UGameUpdateSubsystem* Updater = GameInstance->GetSubsystem<UGameUpdateSubsystem>();
Updater->OnChanged.AddLambda([Updater] { /* redraw from Updater->GetState() */ });
Updater->OnReadyToRestart.AddLambda([World] { UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false); });
if (!GIsEditor) Updater->Check();

// Later, when the player accepts:
if (Updater->CanInstall()) Updater->DownloadAndInstall();
else Updater->OpenReleasePage();
```

| Call | Does |
| --- | --- |
| `Check()` | Asks the source for the latest release. Ignored while busy |
| `IsUpdateAvailable()` | A newer release was found; stays true while it downloads or after a failed download |
| `CanInstall()` | Windows, a writable install folder, and a release with an installer and a checksum |
| `DownloadAndInstall()` | Downloads, verifies and starts the installer |
| `Cancel()` | Stops a check or download and deletes the partial file |
| `OpenReleasePage()` | The release's page in the browser |
| `GetState()`, `GetLatest()`, `GetProgress()`, `GetProgressText()`, `GetMessage()` | What to draw |

States run `Idle → Checking → UpToDate | Available → Downloading → Verifying → Installing`. Any step can end in
`Failed`, with the reason in `GetMessage()`; after a failed download `DownloadAndInstall()` retries.

The subsystem never checks on its own and never quits the game. Your project chooses when to check (once per run in a
packaged build is typical) and quits on `OnReadyToRestart`.

### How an update runs

1. `Check()` reads `https://api.github.com/repos/<Repository>/releases/latest` without a token, then the installer's
   `.sha256`. Only a `User-Agent` of `<Project>/<version>` is sent; no player data.
2. `DownloadAndInstall()` streams the installer to `<temp>/<Project>Update/<version>/`, reporting progress and
   speed.
3. The file's SHA-256 is computed on a worker thread. A mismatch deletes it and nothing runs.
4. The installer starts, detached, with `InstallerArguments`. `OnReadyToRestart` fires, and the game quits so the
   installer can replace its files.
5. The next packaged start deletes the download folder.

In the editor `InstallDir` is empty, so `CanInstall()` is false and nothing can be installed over the engine.

### Other update sources

The subsystem only uses `IGameUpdateSource` (`GameUpdateSource.h`): `FetchLatest`, `Download` and `Cancel`.
`GameUpdate::MakeGitHubSource` is the built-in one. To use your own server or a store, implement the interface and
pass it to `Configure(Config, Source)`. Tests use the same call with a fake source and launcher.

The pieces also work alone: `GameUpdate::IsNewer` (`GameUpdateVersion.h`), `ParseGitHubRelease` and `ParseChecksum`
(`GameUpdateRelease.h`), `Sha256OfFile` (`GameUpdateHash.h`) and `FormatSize` and `ProgressLine` (`GameUpdateFormat.h`).

### Limitations

- Installing is Windows only. Elsewhere `CanInstall()` is false; offer `OpenReleasePage()`.
- The installer must be able to wait for the game to close and restart it afterwards; see UOpenGolf's
  `Assets/Installer/UOpenGolf.iss` for an Inno Setup example.
- Installers are not signed by this plugin. An unsigned installer started by the game runs without a SmartScreen
  prompt, but the same file downloaded in a browser warns.
- Tests: `OpenGameUpdate.*` (version order, release parsing, SHA-256 vectors, and the state machine with a fake
  source). `OpenGameUpdate.Live` reads the real latest release only with `-OpenGameUpdateLive`; add
  `-OpenGameUpdateAsset=<name with {version}>` to check an asset other than the configured installer.
