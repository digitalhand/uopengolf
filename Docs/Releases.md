# Builds and releases

GitHub Actions packages the game for Windows and Linux on your own machines
(self-hosted runners) and attaches tagged versions to a GitHub Release. Nothing starts
automatically. Linux and Windows have separate actions, so you can build whichever
platform has a runner online. No paid hosted build runners are required.

### How the pipeline works

In the Actions tab, choose [**Package Linux**](../.github/workflows/package-linux.yml) or
[**Package Windows**](../.github/workflows/package-windows.yml), then **Run workflow**.
Choose the tag or branch under **Use workflow from**. To build both platforms, run
each action on the same ref; a dual-boot machine can do this at different times.

| Job | Runs on | What it does |
| --- | --- | --- |
| Version | GitHub-hosted Ubuntu | Runs the release-script unit tests and works out the version from the ref; an invalid tag fails here |
| Linux package | Self-hosted, labels `self-hosted, Linux, X64, ue-5.8` | Stamps the version, runs `Assets/Scripts/ue.sh package` (build, cook, stage, 60 s start check), builds the `.deb` and `.rpm` with `Assets/Installer/build_deb.py` and `build_rpm.py` and archives the result |
| Windows package | Self-hosted, labels `self-hosted, Windows, X64, ue-5.8` | The same with `Assets/Scripts/ue.ps1 Package`, plus the installer from `Assets/Installer/build_installer.ps1` |
| Draft release (Linux, Windows) | GitHub-hosted Ubuntu | Tags only, one per platform, as soon as that platform's package succeeds: creates the tag's draft release if needed and uploads that platform's archives with `Assets/Scripts/draft_release.sh` |

Each platform allows one package job at a time, without cancelling an active build.
Both workflows use the same release lock for a tag, so their uploads cannot race.

Each platform job makes a clean checkout in the runner's own work folder. It doesn't
touch your working copy or its `Intermediate/`. The checkout downloads the Git LFS assets
(about 340 MB at 2026-10-27), which counts against the account's GitHub LFS bandwidth. Archives are kept as workflow artifacts
for 14 days. Only the Draft release jobs can write to the repository.

### Runner setup

Register one runner per OS on this repository: Settings → Actions → Runners → **New
self-hosted runner**. Follow GitHub's download and `config` steps, then:

1. When `config` asks for extra labels, enter `ue-5.8`. GitHub adds `self-hosted`, the OS
   and `X64` itself. Use a current runner release: the workflow's actions need Node 24.
2. Create a `.env` file in the runner folder with the engine location, for example
   `UE_ROOT=/path/to/UE_5.8` on Linux. On Windows, `UE_ROOT` is optional when the
   engine is in `C:\Program Files\Epic Games\UE_5.8`.
3. Keep the runner installation and its work folder separate from your development
   checkout. Checkout cleans this folder, including ignored files. Never point it at
   your working copy or copy local-only plugins or local config into it.
4. Start the runner with `./run.sh` (Linux) or `run.cmd` (Windows) when you want to
   package, and stop it with Ctrl+C afterwards. Installing it as a service is optional.
5. After an engine patch update, point each runner's `UE_ROOT` at the new installation
   and update `ENGINE_VERSION` in `Assets/Scripts/check_package_runner.py`.

| | Linux | Windows |
| --- | --- | --- |
| Engine | Unreal Engine 5.8.4, installed or source build | Unreal Engine 5.8.4 |
| Tools | Engine's Clang 20.1.8 toolchain, Git with Git LFS, Bash, Python 3.10 or later, GNU tar, GNU coreutils, `dpkg-deb` (Ubuntu's `dpkg` package) and `rpmbuild` (`sudo apt install rpm`) | Git for Windows (includes Git LFS), Windows PowerShell 5.1, Visual Studio 2022 with the SDKs under [Prerequisites](Development.md#windows), and Inno Setup 6 (`winget install JRSoftware.InnoSetup`) |
| Work folder | Default | Short, for example `--work C:\gh` during `config`; Unreal paths can exceed Windows path limits |

Before packaging, `Assets/Scripts/check_package_runner.py` checks the engine version, build
tools (including Inno Setup on Windows), Git LFS assets and absence of local `Config/User*.ini` files.
It also checks for Development, DebugGame and command-line Unreal editors anywhere on
the machine. Close the editor before starting a build and leave it closed until the
job finishes. A running editor stops the job; the workflow never closes it for you.
Packaging and the startup check do not measure rendered FPS.

The optional ModelContextProtocol, AllToolsets and Terminal editor plugins are not
required on runners. They remain enabled for editor use when installed locally.

### Public repository safety

This repository is public, so anyone can open a pull request. A self-hosted runner runs
whatever its workflow asks, so:

- Both package workflows start only from **Run workflow**, which needs write access. Never
  add `pull_request` or `pull_request_target` triggers to it, or to any workflow that uses
  the `ue-5.8` runners: a fork could change the workflow in its pull request and run code
  on your machine.
- In both the organization and the repository settings, go to Settings → Actions →
  General. In the fork pull request approval setting, choose the option that requires
  approval for **all** outside (external) contributors, not just first-time ones. A fork's
  pull request can add its own workflow aimed at these runners, and approval stops it
  from running until you've read it.
- Anyone with write access can run the workflow from any branch, and the workflow file on
  that branch is what runs on your machine. If you add collaborators, consider putting the
  package jobs behind an Environment with required reviewers.
- The package jobs get a read-only token, and checkout doesn't leave credentials in the
  work folder. Third-party actions are pinned to full commit SHAs.
- Keep runners stopped when you aren't packaging. Running a runner under a separate,
  non-admin OS account limits what a job can reach. This is optional; that account needs
  read access to the engine.

### Versioning

The HUD version (`ProjectVersion` in `Config/DefaultGame.ini`) comes from the ref you
build:

| Ref | Version | Result |
| --- | --- | --- |
| Tag `v1.2.3` | `1.2.3` | Workflow artifacts and a draft release |
| Tag `v1.2.3-rc.1` | `1.2.3-rc.1` | The same, marked as a prerelease |
| Branch, such as `main` at commit `cd5c3b8` | Committed version plus `-dev.` and the short commit, such as `0.0.1-dev.cd5c3b8` | Workflow artifacts only |

The job writes the version into its own checkout; nothing is committed. Release tags are
`v` followed by `MAJOR.MINOR.PATCH`, with an optional `-prerelease`. Build metadata
(`+...`) is rejected. `Assets/Scripts/project_version.py` does the checking and stamping. To see
what a ref would produce:

```bash
python3 Assets/Scripts/project_version.py for-ref tag v1.2.3 "$(git rev-parse HEAD)"
```

### Making a release

1. Make sure the release commit contains both workflows. **Run workflow** only appears
   once the workflows are on `main`.
2. Tag the commit and push the tag:

   ```bash
   git tag -a v0.1.0 -m "v0.1.0"
   git push origin v0.1.0
   ```

3. Start the runner on the OS you're using, then go to Actions → **Package Linux** or
   **Package Windows** → **Run workflow**. Under *Use workflow from*, choose
   **Tags → v0.1.0** and run it.
4. When you're on the other OS, start its runner and run the other action on the same
   tag. Its archives join the same draft. While the release is a draft,
   running a platform again replaces its archives. Once it's published, a run can still
   add a missing platform but never replaces existing files.
5. Open the draft under **Releases**, check both platforms' assets and the generated
   notes, then **Publish release**.

A build always uses the tagged commit. To ship a fix, tag a new version (for example the
next `-rc`); don't move a tag that has already been published.

### Test builds

To check packaging without making a release, run the workflow on a branch such as
`main`. The archives appear under the run's **Artifacts** for 14 days, with a version
like `0.0.1-dev.cd5c3b8`. This repository is public, so any signed-in GitHub user can
download those artifacts too.

### Release assets

| File | Contents |
| --- | --- |
| `UOpenGolf-<version>-Linux-amd64.deb` | Ubuntu installer: application-menu entry, package-manager upgrades and removal; administrator authorization required to install |
| `UOpenGolf-<version>-Linux-x86_64.rpm` | The same for Fedora and openSUSE: [Fedora and openSUSE installer](#fedora-and-opensuse-installer) |
| `UOpenGolf-<version>-Linux-x64.tar.gz` | The Linux package in a folder of the same name, for any distribution; run `UOpenGolf.sh` |
| `UOpenGolf-<version>-Linux-x64-symbols.tar.gz` | `.debug` files for gdb, in the same folder layout |
| `UOpenGolf-<version>-Windows-x64-Setup.exe` | The Windows installer: installs, upgrades an existing install, and is what the game downloads for an [in-game update](#in-game-updates) |
| `UOpenGolf-<version>-Windows-x64.zip` | The same Windows package as a portable folder of the same name; run `UOpenGolf.exe` |
| `UOpenGolf-<version>-Windows-x64-symbols.zip` | `.pdb` files, in the same folder layout |
| `*.sha256` | The SHA-256 of each archive and installer; check with `sha256sum -c FILE.sha256`. In-game updates need these |
| `SHA256SUMS`, `SHA256SUMS.asc` | Every file's SHA-256, signed by hand with the release key ([signing](../Assets/Installer/CodeSigning.md)). In-game `.deb` and `.rpm` updates need both |

- **Build type:** packages are **Development** builds, with the console and logs, like the
  local `Package` action.
- **Symbols:** to debug, extract a symbols archive over its package. Linux `.sym` files stay
  in the main archive, because the engine uses them to print readable crash call stacks.
- **Licenses:** each package includes `LICENSE` and `THIRD_PARTY_NOTICES.md`, as well as the
  engine's `NOTICES.txt`.
- **Content:** builds reject local config files before cooking. Use a clean runner checkout.
- **Icons and splash:** these must be committed; the runner only has what is in Git.
  - `Build/Windows/Application.ico` is compiled into both Windows executables and is also the Setup icon.
  - `Content/Splash/Splash.bmp` is the game splash on both platforms. It must be a BMP: packaging stages only
    `Splash.bmp`, and packaged Linux builds can't read PNG. If the editor's **Game Splash** setting is given a PNG,
    it replaces the BMP with `Splash.png`, and packages then show no splash.
  - `Content/Splash/Icon.bmp` (64×64, 32-bit) is the Linux window icon.
  - The `.deb` application-menu icon comes from `Assets/Images/Icons/png/UOpenGolf_<size>.png`.
  - `Build/Linux/Application.png` only appears in the editor's settings; packaging doesn't use it.

### Ubuntu installer

The `.deb` targets Ubuntu 24.04 amd64. Open it with a graphical package installer;
the available handler depends on your Ubuntu desktop installation. It asks for administrator
authorization, without requiring `chmod`. Alternatively, from the download folder:

```bash
sudo apt install ./UOpenGolf-0.1.0-Linux-amd64.deb
```

Launch **UOpenGolf** from the application menu, or run `uopengolf`. Install a newer `.deb`
the same way to upgrade. Close the game first. Remove it through your package manager or
`sudo apt remove uopengolf`. The package owns `/opt/uopengolf`, `/usr/bin/uopengolf`, the
application-menu entry, its icons in `/usr/share/icons/hicolor` and the AppStream metadata
in `/usr/share/metainfo`. Settings and logs use `~/.config/Epic/UOpenGolf/Saved`;
recordings keep their configured location. Removal and purge preserve those user files.
Portable installations are not migrated automatically. The Vulkan/GPU requirements in
[Development](Development.md) still apply; the package does not install graphics drivers.

Build the installer without root, after `bash Assets/Scripts/ue.sh package`:

```bash
python3 -B Assets/Installer/build_deb.py --package-dir Packaged/Linux --version 0.1.0 --output-dir Packaged/dist
```

Use the version stamped into the cooked build. The builder does not cook or update game
content. It includes project and third-party notices, keeps runtime `.sym` files, excludes
`.debug` files and `Saved` folders, and writes a `.sha256` beside the `.deb`. Debian metadata
maps prereleases such as `0.1.0-rc.1` to `0.1.0~rc.1`, so the final release sorts after them.
The workflow produces this installer alongside the portable archive and symbols archive.

The package also installs `/usr/lib/uopengolf/install-update`, a polkit action for it in
`/usr/share/polkit-1/actions`, the release key's public half in
`/usr/share/uopengolf/release-signing-key.gpg`, and `/opt/uopengolf/.package-format`. They let
the game install its own updates; see [In-game updates](#in-game-updates). The package depends
on `pkexec` for that prompt and on `gpgv` to check the release's signature.

Software centres take the installed game's name, summary, developer, licence, icon and
screenshots from `Assets/Installer/uopengolf.metainfo.xml`. The builder fills in the release
version and date. Its screenshots are `main`-branch URLs of files in `Assets/Images`, so keep
those files, or update the URLs. Its `<pkgname>` must stay equal to the package name: KDE
Discover ignores AppStream data that names no package. Opening a downloaded `.deb` in Discover
still shows the file name up to its first dot, a generic icon and an unknown licence. That
view reads the file name and PackageKit's details, never the package's metadata, and PackageKit
reports every Ubuntu package's licence as unknown.

### Fedora and openSUSE installer

The `.rpm` targets x86_64 Fedora and openSUSE. It installs the same files as the `.deb`:
`/opt/uopengolf`, `uopengolf` on the path, the application-menu entry, icons, AppStream
metadata and the in-game update helper. Install or upgrade it from the download folder:

```bash
sudo dnf install ./UOpenGolf-0.3.0-Linux-x86_64.rpm      # Fedora
sudo zypper install --allow-unsigned-rpm ./UOpenGolf-0.3.0-Linux-x86_64.rpm   # openSUSE
```

Remove it with `sudo dnf remove uopengolf` or `sudo zypper remove uopengolf`. Settings and
recordings stay in `~/.config/Epic/UOpenGolf/Saved`. The package declares its libraries by name
(`libvulkan.so.1` and so on), not by package name, so each distribution's package manager
resolves them from its own repositories. It isn't signed with an RPM key; check the download
against its `.sha256` (or the signed `SHA256SUMS`). Prereleases map to versions such as
`0.1.0~rc.1`, as in the `.deb`.

Ubuntu's `rpm` package builds it on the Linux runner after `ue.sh package`:

```bash
python3 -B Assets/Installer/build_rpm.py --package-dir Packaged/Linux --version 0.1.0 --output-dir Packaged/dist
```

`build_deb.py` and `build_rpm.py` share their staging in `Assets/Installer/linux_payload.py`.
To check an install, upgrade and removal, and the update helper's signature checks, in disposable
containers (each signs its fixture packages with a throwaway key):

```bash
docker run --rm -v "$PWD":/src:ro ubuntu:24.04 bash /src/Assets/Scripts/check_deb_install.sh
docker run --rm -v "$PWD":/src:ro fedora:latest bash /src/Assets/Scripts/check_rpm_install.sh
docker run --rm -v "$PWD":/src:ro opensuse/leap:15.6 bash /src/Assets/Scripts/check_rpm_install.sh
```

Add a real package (`-v "$PWD/Packaged/dist":/packages:ro` and its path as the argument) to
also resolve every shipped library and start the game.

### In-game updates

Packaged games check this repository's latest **published** release once per run, through the
[OpenGameUpdate](../Plugins/OpenGameUpdate/README.md) plugin. Drafts and prereleases are never
offered. When the release's version is newer than the game's `ProjectVersion`, the menu shows
**Update to v…**. On Windows the game then downloads the release's Setup, checks it against its
`.sha256`, starts it silently on the game's own folder and quits; Setup waits for the game to
close, replaces its files, keeps `Saved\` (settings, sign-in, logs) and starts the new version.

On Linux the game picks the download that matches how it was installed, then starts the
plugin's update helper and quits:

| Installed from | Downloads | Then |
| --- | --- | --- |
| `.deb` (Ubuntu, Debian) | `SHA256SUMS`, `SHA256SUMS.asc`, then `UOpenGolf-<version>-Linux-amd64.deb` | The desktop asks for an administrator password ("Authentication is required to update UOpenGolf"), then `install-update` checks the signature and installs it with apt |
| `.rpm` (Fedora, openSUSE) | `SHA256SUMS`, `SHA256SUMS.asc`, then `UOpenGolf-<version>-Linux-x86_64.rpm` | The same prompt, then `install-update` checks the signature and installs it with dnf or zypper |
| `.tar.gz` (any distribution) | `UOpenGolf-<version>-Linux-x64.tar.gz` | No prompt. The archive is unpacked beside the game's folder, `Saved/` is moved across, and the folders are swapped |

The game starts again either way. If the password prompt is cancelled or the install fails, the
old version starts and its update screen says why. `install-update` copies the files to a
root-only folder before checking them. It installs only a package whose SHA-256 and file name are
in a `SHA256SUMS` signed with the release key it shipped with, named `uopengolf`, and newer than the
installed one. Without the signature, any program running as the player could raise the same
password prompt for a package of its own. A release published without `SHA256SUMS.asc` therefore
shows `.deb` and `.rpm` players the release page instead; sign before you publish. The download stays
in `~/.cache/UOpenGolfUpdate`, which only the player can open, until the next start.
A portable folder that the player can't write to, or whose parent folder they can't write to, gets
the release page instead.

The installer (`Assets/Installer/UOpenGolf.iss`, Inno Setup 6) installs per user, without an
administrator prompt, to `%LOCALAPPDATA%\Programs\UOpenGolf`, with a Start menu entry and an
uninstaller in **Apps**. Its `AppId` must never change: it is how a newer Setup finds an
existing install. To build one locally after `ue.ps1 Package`:

```powershell
powershell -File Assets\Installer\build_installer.ps1 -PackageDir Packaged\Windows -Version 0.1.0 -OutputDir Packaged\dist
```

The updater relies on these release details, so keep them stable:

- The asset name `UOpenGolf-<version>-Windows-x64-Setup.exe`, set in `[OpenGameUpdate]` in
  `Config/DefaultGame.ini`, and its `.sha256` in `sha256sum` format.
- The Linux asset names `UOpenGolf-<version>-Linux-amd64.deb`, `-Linux-x86_64.rpm` and
  `-Linux-x64.tar.gz`, set in `Config/Linux/LinuxGame.ini`, each with its `.sha256`.
- The package name `uopengolf`, `/usr/lib/uopengolf/install-update` and the `.package-format` marker.
- `SHA256SUMS` and `SHA256SUMS.asc`, signed with the release key that installed copies carry. Ship a
  renewed or replaced key in a release before the old one stops working; see
  [CodeSigning.md](../Assets/Installer/CodeSigning.md#in-game-updates-check-the-signature).
- The tag `v<version>`, matching the stamped `ProjectVersion`.
- Silent install flags in the same config. Setup must accept `/DIR=` for the game's folder.

A zip install updates the same way: Setup installs over the zip's folder and from then on it is
a normal installed copy. A folder the game can't write to, such as one under Program Files,
gets the release page instead.

Releases from before this feature can't update themselves; players install the first release
that has a Setup by hand. Releases up to and including v0.1.0 can't either: their config lost the
braces in `{version}`, so they look for an asset named `UOpenGolf-version-Windows-x64-Setup.exe`
and fall back to the release page. The value is now quoted, and `OpenGameUpdate.Config` checks that
it keeps its placeholder. On Linux, releases up to and including v0.2.0 only offer the release page:
install the first release with Linux updates by hand once, and later releases update from the game.

### Course packs

Downloadable courses are released separately from the game, as content-only packs for one game version:

```bash
python3 Assets/Scripts/package_course.py --engine <engine> --platform Linux --version draft3 --host <host folder>
python3 Assets/Scripts/check_course_install.py <packaged game>/UOpenGolf.sh <host folder> Saved/Automation/CourseInstall
```

`package_course.py` cooks the base game for the version in `Config/DefaultGame.ini`, then the course from its content
plugin, and writes `<id>/<version>/<platform>/Course.{pak,utoc,ucas}` with their sizes and SHA-256 hashes into the host
folder's `catalog.json`. It also writes the course's derived layout and author corrections to `<id>/<version>/layout/`,
because that data comes from OpenStreetMap and is published under the Open Database License. A version that already
exists is never overwritten; publish a new one. `check_course_install.py` serves the folder locally and runs the
packaged game through absent, install (and play at once), offline play, uninstall and absent again.

The game reads the catalog from `[CourseDownloads] BaseUrl` in the game config (HTTPS). Until a host is chosen,
nothing is listed. The same folder can later be served from S3 through CloudFront. Only Linux packs have been built
and checked so far; see [Course Designer](CourseDesign/course_designer.md).

### Limitations and troubleshooting

- If a platform's runner is offline, its job waits in the queue, and GitHub cancels it
  after 24 hours. Start the runner, or cancel that platform's run. The other platform's
  action runs independently.
- If packaging fails, the run uploads a `package-logs-<platform>` artifact with
  `Saved/Logs/` (including `Package.log`) and the start-check report, kept for 7 days. Like
  other artifacts it is public, and it contains the runner's local paths and machine name.
- The start check writes a `Saved/` folder inside the package. The workflow deletes it
  before archiving, so releases don't carry the runner's log or settings.
- Shipping builds and code signing are not set up. Windows archives and the installer are
  unsigned, so SmartScreen may warn when one is downloaded in a browser. In-game updates
  are not affected.
- Not yet verified: full runs of the separate workflows on GitHub, and `ue.ps1 Package`
  itself. The installer's install, locked-game upgrade and uninstall were checked with a
  stand-in package (2026-11-12); a full in-game update needs the first release that has a
  Setup asset.
- Linux in-game updates (2026-11-24): the helper's archive swap, rollback and pkexec handling
  are unit-tested. `install-update` was checked as root in Ubuntu 24.04, Fedora 45 and openSUSE
  Leap 15.6 and Tumbleweed containers with signed fixture packages. It refused links, FIFOs,
  tampered or unlisted packages, other packages, other keys, an edited list and older versions. It
  accepted an expired but genuine signing key. Not yet verified: a real polkit prompt, the `.rpm` of a
  real build, the helper staged in a packaged game, and a full update from one signed, published
  release to the next.
