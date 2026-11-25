# OpenGameUpdate reference

### Configuration

All project-specific values are in `[OpenGameUpdate]` in `Config/DefaultGame.ini`:

```ini
[OpenGameUpdate]
Repository=owner/name
InstallerAsset="MyGame-{version}-Windows-x64-Setup.exe"
InstallerArguments=/SILENT /SUPPRESSMSGBOXES /NORESTART /DIR="{installdir}"
```

Unreal's ini reader drops unquoted `{` and `}`, so an unquoted `MyGame-{version}-...` reaches the game as
`MyGame-version-...` and no release ever matches. Put the whole value in quotes, as above. In `InstallerArguments`
the placeholder is already inside quotes. `FGameUpdateConfig::Load` logs a warning when either placeholder is
missing.

| Key | Meaning |
| --- | --- |
| `Repository` | GitHub `owner/name`. Its latest published release is the update; drafts and prereleases are never offered |
| `InstallerAsset` | The installer's asset name. `{version}` is the release tag without its leading `v` |
| `InstallerArguments` | The installer's command line. `{installdir}` is the running game's folder, without a trailing slash |

Linux reads the same section with its platform overrides, usually from `Config/Linux/LinuxGame.ini`:

```ini
[OpenGameUpdate]
InstallerAsset="MyGame-{version}-Linux-x64.tar.gz"
PackageAsset_deb="MyGame-{version}-Linux-amd64.deb"
PackageAsset_rpm="MyGame-{version}-Linux-x86_64.rpm"
PackageInstaller=/usr/lib/mygame/install-update
ChecksumListAsset=SHA256SUMS
ChecksumSignatureAsset=SHA256SUMS.asc
```

| Key | Meaning |
| --- | --- |
| `InstallerAsset` | On Linux, the portable archive (`.tar.gz`) that replaces a portable copy's folder |
| `PackageAsset_<format>` | The system package downloaded by a copy that a package of that format installed, such as `deb` or `rpm` |
| `PackageInstaller` | The root-owned program that `pkexec` runs as `PackageInstaller <package> <list> <signature>` |
| `ChecksumListAsset` | The release's checksum list in `sha256sum` format, listing the package. Package installs require it |
| `ChecksumSignatureAsset` | The release's detached OpenPGP signature of that list. Package installs require it |

A Linux copy counts as package-installed when its folder holds `.package-format`, one word such as `deb`, and
`PackageAsset_<word>` is set. Only the package installs that file. Any other packaged copy is portable. The
plugin never reads dpkg's or rpm's database, so it works the same on every distribution.

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
| `CanInstall()` | The copy's install kind is ready (see [Linux](#linux)), and the release has its download and a checksum |
| `GetInstallBlocker()` | Why `CanInstall()` is false, as one sentence to show players; empty when it is true |
| `GetInstallKind()` | `Installer` (Windows), `Archive` (portable Linux), `Package` (Linux system package; asks for the password) or `None` |
| `GetLastInstallFailure()` | Linux: why the update before this start didn't install, such as a cancelled password prompt; empty otherwise |
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

#### Linux

The download goes to `$XDG_CACHE_HOME/<Project>Update/` (or `~/.cache/…`), a folder only the player can open:
Unreal's own temporary folder on Linux is the shared `/var/tmp`, and a package from there is installed as root.
After verification the game starts the plugin's `Resources/Linux/apply-update.sh` with `/bin/sh`, staged with the
game, and quits. The helper waits for the game to exit, then:

| Install kind | What the helper does | Password |
| --- | --- | --- |
| `Archive` | Checks the archive has one top-level folder and no absolute or `..` paths, unpacks it beside the game, moves every `<folder>/*/Saved` across, swaps the folders and deletes the old one. The game's folder and its parent must be writable | No |
| `Package` | Runs `pkexec <PackageInstaller> <package> <list> <signature>`. The desktop's polkit agent asks for an administrator password | Yes |

For a package install the game first downloads `ChecksumListAsset` and `ChecksumSignatureAsset`, before the package.
A release without both shows `This release isn't signed, so it can't be installed as administrator.` A list that
doesn't give the package's verified SHA-256 under its asset name fails before the package downloads.

Either way it writes `ok <version>` or `failed <reason>` to `result` in the download folder and starts the game again,
with its original command line, even when the update failed. The next start reads the result into
`GetLastInstallFailure()` before it deletes the folder. If the game doesn't close within 120 seconds, nothing is
changed and the game isn't started a second time. While it works, the helper keeps its process id in
`helper.pid` in the download folder. A game started meanwhile leaves the download alone, and a new update waits
until it finishes.

`PackageInstaller` belongs to the project's package, not the plugin. It runs as root, and any program running as
the player can start it with files of its choosing. It should therefore:

- Copy each file, without following links, somewhere only root can write, and check only the copies.
- Check the list's signature against a public key the package installs, and require the package's SHA-256 under its
  file name in that list. Without this, a malicious program could raise the game's genuine password prompt for a
  package of its own.
- Accept only the project's own package name and a newer version, then install it with the system's package
  manager.

Ship it with a polkit action whose `org.freedesktop.policykit.exec.path` names it, so the prompt names the game. UOpenGolf's
`Assets/Installer/install-update` and `io.github.digitalhand.uopengolf.update.policy` are an example for `.deb`
(apt) and `.rpm` (dnf or zypper). They check the signature with `gpgv` and the release key in
`/usr/share/uopengolf/release-signing-key.gpg`.

The helper uses POSIX `sh`, `tar`, `gzip`, `awk` and util-linux `setsid` (it falls back to `nohup`), so portable
copies update on any distribution. Package installs also need `pkexec` and a running polkit agent, which desktop
environments provide.

`LogGameUpdate` records each check (`Latest release <x>, this build <y>.`). When an update is found but can't be
installed, it warns with the reason: the asset name it looked for and didn't find, the checksum's HTTP code, or an
unwritable install folder.

In the editor `InstallDir` is empty, so `CanInstall()` is false and nothing can be installed over the engine.

### Other update sources

The subsystem only uses `IGameUpdateSource` (`GameUpdateSource.h`): `FetchLatest`, `Download` and `Cancel`.
`GameUpdate::MakeGitHubSource` is the built-in one. To use your own server or a store, implement the interface and
pass it to `Configure(Config, Source)`. Tests use the same call with a fake source and launcher.

The pieces also work alone: `GameUpdate::IsNewer` (`GameUpdateVersion.h`), `ParseGitHubRelease` and `ParseChecksum`
(`GameUpdateRelease.h`), `Sha256OfFile` (`GameUpdateHash.h`) and `FormatSize` and `ProgressLine` (`GameUpdateFormat.h`).

### Limitations

- Installing works on Windows and Linux. Elsewhere, and in the editor, `CanInstall()` is false; offer
  `OpenReleasePage()`.
- On Linux, a package install without a polkit agent (some minimal window managers) can't show the password prompt.
  The old version starts again and reports it; install the package by hand.
- The installer must be able to wait for the game to close and restart it afterwards; see UOpenGolf's
  `Assets/Installer/UOpenGolf.iss` for an Inno Setup example.
- Installers are not signed by this plugin. An unsigned installer started by the game runs without a SmartScreen
  prompt, but the same file downloaded in a browser warns.
- Tests: `OpenGameUpdate.*` (version order, release parsing, SHA-256 vectors, the state machine with a fake
  source, Linux install detection, the signed-list downloads and helper arguments). UOpenGolf's `Assets/Scripts/test_apply_update.py`
  runs the Linux helper itself against temporary folders. `OpenGameUpdate.Live` reads the real latest release only with `-OpenGameUpdateLive`; add
  `-OpenGameUpdateAsset=<name with {version}>` to check an asset other than the configured installer.
