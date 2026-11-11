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
| Linux package | Self-hosted, labels `self-hosted, Linux, X64, ue-5.8` | Stamps the version, runs `Scripts/ue.sh package` (build, cook, stage, 60 s start check) and archives the result |
| Windows package | Self-hosted, labels `self-hosted, Windows, X64, ue-5.8` | The same with `Scripts/ue.ps1 Package` |
| Draft release (Linux, Windows) | GitHub-hosted Ubuntu | Tags only, one per platform, as soon as that platform's package succeeds: creates the tag's draft release if needed and uploads that platform's archives with `Scripts/draft_release.sh` |

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
   and update `ENGINE_VERSION` in `Scripts/check_package_runner.py`.

| | Linux | Windows |
| --- | --- | --- |
| Engine | Unreal Engine 5.8.4, installed or source build | Unreal Engine 5.8.4 |
| Tools | Engine's Clang 20.1.8 toolchain, Git with Git LFS, Bash, Python 3.10 or later, GNU tar and GNU coreutils | Git for Windows (includes Git LFS), Windows PowerShell 5.1, and Visual Studio 2022 with the SDKs under [Prerequisites](Development.md#windows) |
| Work folder | Default | Short, for example `--work C:\gh` during `config`; Unreal paths can exceed Windows path limits |

Before packaging, `Scripts/check_package_runner.py` checks the engine version, build
tools, Git LFS assets and absence of local `Config/User*.ini` files.
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
(`+...`) is rejected. `Scripts/project_version.py` does the checking and stamping. To see
what a ref would produce:

```bash
python3 Scripts/project_version.py for-ref tag v1.2.3 "$(git rev-parse HEAD)"
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
| `UOpenGolf-<version>-Linux-x64.tar.gz` | The Linux package in a folder of the same name; run `UOpenGolf.sh` |
| `UOpenGolf-<version>-Linux-x64-symbols.tar.gz` | `.debug` files for gdb, in the same folder layout |
| `UOpenGolf-<version>-Windows-x64.zip` | The Windows package in a folder of the same name; run `UOpenGolf.exe` |
| `UOpenGolf-<version>-Windows-x64-symbols.zip` | `.pdb` files, in the same folder layout |
| `*.sha256` | The SHA-256 of each archive; check with `sha256sum -c FILE.sha256` |

- **Build type:** packages are **Development** builds, with the console and logs, like the
  local `Package` action.
- **Symbols:** to debug, extract a symbols archive over its package. Linux `.sym` files stay
  in the main archive, because the engine uses them to print readable crash call stacks.
- **Licenses:** each package includes `LICENSE` and `THIRD_PARTY_NOTICES.md`, as well as the
  engine's `NOTICES.txt`.
- **Content:** builds reject local config files before cooking. Use a clean runner checkout.

### Limitations and troubleshooting

- If a platform's runner is offline, its job waits in the queue, and GitHub cancels it
  after 24 hours. Start the runner, or cancel that platform's run. The other platform's
  action runs independently.
- If packaging fails, the run uploads a `package-logs-<platform>` artifact with
  `Saved/Logs/` (including `Package.log`) and the start-check report, kept for 7 days. Like
  other artifacts it is public, and it contains the runner's local paths and machine name.
- The start check writes a `Saved/` folder inside the package. The workflow deletes it
  before archiving, so releases don't carry the runner's log or settings.
- Shipping builds and code signing are not set up. Windows archives are unsigned, so
  SmartScreen may warn on first launch.
- Not yet verified: full runs of the separate workflows on GitHub, and `ue.ps1 Package`
  itself.
