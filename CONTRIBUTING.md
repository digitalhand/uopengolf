# Contributing to Unreal Open Golf

Thank you for helping. Bug reports, fixes, features, documentation, tests and
redistributable assets are all welcome. This guide covers how to propose a change and
what a pull request needs before it can be merged.

## Table of contents

- [Before you start](#before-you-start)
- [Set up](#set-up)
- [Branches and commits](#branches-and-commits)
- [Code style](#code-style)
- [Tests](#tests)
- [Physics changes](#physics-changes)
- [Content, assets and data](#content-assets-and-data)
- [Documentation](#documentation)
- [Pull requests](#pull-requests)
- [Workflows and runners](#workflows-and-runners)
- [Reporting bugs](#reporting-bugs)
- [Security issues](#security-issues)
- [Licence](#licence)

## Before you start

- See [Licence](#licence) section. 
- Search existing issues and pull requests first.
- For anything bigger than a small fix, open an issue describing the problem and your
  proposed approach before writing code. Agreeing on the direction early saves rework.
- Say in the issue if you plan to edit the map (`/Game/UOpenGolf/Maps/UOpenGolf`) or other
  `.uasset` files. Unreal assets are binary and can't be merged, so two people editing the
  same asset at once means one of them redoes their work.
- Be respectful and constructive in issues, reviews and discussions.

## Set up

Follow [Get started](README.md#get-started) and [Development](Docs/Development.md). In short:

1. Install Unreal Engine **5.8.4** with its C++ toolchain, and Git LFS (`git lfs install`).
2. Fork the repository, clone your fork and run `git lfs pull` if assets were not downloaded.
3. Build the Development Editor target, then run the smoke test to confirm the checkout works:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Build
   powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Smoke
   ```

   On Linux: `bash Assets/Scripts/ue.sh build`, then `bash Assets/Scripts/ue.sh smoke`.

Keep machine-specific files out of your changes: `.vscode/`, `*.code-workspace`,
`Config/User*.ini`, and the `.uproject`'s `EngineAssociation` (see
[Engine version warning](Docs/Development.md#engine-version-warning)).

## Branches and commits

- Branch from an up-to-date `main`. Name the branch after the change, with a
  `feature/`, `fix/` or `docs/` prefix, for example `fix/linux-video-display-settings`.
- Keep each branch to one logical change. Unrelated fixes, formatting sweeps and
  refactors belong in their own pull requests.
- Write commit subjects as `<type>: <summary>`, using `feature`, `fix` or `docs`, for
  example `fix: keep the ball on the tee after a safety stop`. Keep the subject short;
  use the body to explain *why* when it isn't obvious.
- Rebase on `main` or merge it in to resolve conflicts before review. Don't rewrite history
  on a branch after review has started without saying so.

## Code style

The repository's `.editorconfig` applies to every file: UTF-8, LF line endings, a final
newline, and 4-space indentation (2 for JSON, Markdown and `.uproject`).

### C++

- Follow Epic's [C++ coding standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine):
  type prefixes (`F`, `U`, `A`, `S`, `E`, `I`, `T`), PascalCase names and `b` for booleans.
- Format with the repository's `.clang-format`. Prefer `git clang-format` so only the lines
  you changed are reformatted. Never reorder includes: `*.generated.h` must stay last.
- `.clang-tidy` holds a conservative set of checks. Run it on files you change when you can;
  it needs a compilation database (`ue.ps1 CompileCommands` or `ue.sh compilecommands`).
- Start new source files with `// SPDX-License-Identifier: MIT`.
- Put new code in the module or plugin that owns the behavior; see
  [Architecture](Docs/Architecture.md) and [plugins](Plugins/README.md). Plugins don't
  depend on the game module.
- Fix warnings your change introduces. Remove dead code rather than commenting it out.

### Python and shell scripts

- Start new files with `# SPDX-License-Identifier: MIT`.
- Scripts that don't need Unreal should stay engine-free, so their tests run without the editor.
- Put tests beside the script as `test_<name>.py`, using `unittest`.
- Scripts and tests that use the local reference data must skip cleanly when it is absent
  (see [Content, assets and data](#content-assets-and-data)).
- `Assets/Scripts/ue.ps1` and `Assets/Scripts/ue.sh` expose the same actions. When you add or
  change an action in one, do the same in the other and update the table in
  [Development](Docs/Development.md#build-run-and-test).

### Names

Use generic names for third-party products, brands and organizations in code, tests,
scripts and documentation, for example "launch-monitor export" or "tour averages". The
simulator connector protocol is called **Open Connect**.

## Tests

[Testing](Docs/Testing.md) maps each area to its automation suite. Before opening a pull request:

1. Build the editor target for the configuration you test.
2. Run the suites for the behavior you changed, for example
   `ue.ps1 Test -Filter UOpenGolf.Range` or `ue.sh test --filter UOpenGolf.Range`.
   Run `Smoke` as well for any change that can affect startup.
3. If you changed scripts, run their unit tests from the repository root
   (`python3` on Linux):

   ```bash
   python -m pip install -r Assets/Scripts/requirements-test.txt
   python -m unittest discover -s Assets/Scripts -p "test_*.py"
   ```

   Plugin script suites are listed in [Testing](Docs/Testing.md#headless-checks).
4. For rendering, terrain, vegetation or HUD changes, also check the result visually with
   rendering enabled. Headless runs use NullRHI and don't test rendering.

Guidelines for tests:

- Extend an existing suite when you can. Add a test when it protects against a distinct failure,
  not to restate the implementation.
- Name automation tests under the existing `UOpenGolf.*` prefixes, and put them in the owning
  module's `Private/Tests/` folder.
- An empty run or a run with failures isn't a pass. The runner checks the report in
  `Saved/Automation/` for you.
- Run one editor at a time, including automation runs, and close an interactive editor
  before starting an automated one.

## Physics changes

`UOpenGolf.Physics.GameplayPins` pins the results of a fixed set of shots. If your change
moves a pin, the physics has changed.

- If you didn't mean to change the physics, fix the change, not the pins.
- If you did, say so in the pull request, explain why, and include before-and-after numbers.
  Update the pins in the same pull request.
- For flight, bounce, roll or turf changes, run the accuracy scripts described in
  [physics accuracy scripts](Docs/Development.md#physics-accuracy-scripts) if you have the
  local reference data, and include the summary.
- Update the physics [reference](Plugins/OpenFairwayPhysics/Docs/Reference.md) and
  [accuracy](Plugins/OpenFairwayPhysics/Docs/Accuracy.md) docs when the model or its results change.

## Content, assets and data

Read [Licensing](Docs/Licensing.md) before adding any asset.

- Only add content you have the right to release: your own work under MIT, or third-party
  work whose licence allows redistribution. Paying for or modifying an asset doesn't grant that right.
- Never commit purchased or restricted packs or anything derived from them, and don't
  force-add ignored files.
- For every third-party import, record the source URL, pinned version, licence, required
  attribution and destination in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- Include editable sources (for example `.blend` files) for original assets when you have them.
- `.uasset` and `.umap` files go through Git LFS automatically. Check that `git lfs status`
  lists them before you push.
- Keep assets small. Large textures and meshes count against the repository's LFS storage
  and bandwidth.
- Third-party reference data, accuracy baselines and research papers stay out of the
  repository, in the local data folder (`UOPENGOLF_DATA`, default `~/data/uopengolf`).
  See [Assets/Data](Assets/Data/README.md#where-the-data-lives).
- Never commit generated output: `Binaries/`, `Intermediate/`, `DerivedDataCache/`,
  `Saved/` and `Packaged/` are ignored for a reason.

## Documentation

- Update the relevant guide in the same pull request when behavior, controls, commands or
  requirements change. The [README](README.md) lists the guides.
- Write plain, direct English. Describe what the project does now. Leave roadmaps, phase
  plans and dated progress logs out of the documentation.
- Keep commands copy-pasteable from the repository root, and show both Windows and Linux
  forms when they differ.
- Check that the links you add or touch still resolve.

## Pull requests

Open the pull request against `main`. Reviewers listed in `.github/CODEOWNERS` are requested
automatically. In the description:

- Say what changed and why, and link the issue (`Fixes #123`).
- List the tests you ran, with the engine version, OS and result. For example:
  "UE 5.8.4, Windows 11: `Test -Filter UOpenGolf.Range` passed (14 tests)".
- Add screenshots or a short clip for visual or HUD changes.
- Call out anything not verified, such as hardware, packaging or the other platform.
- Note any change to physics pins, assets, licences or third-party notices.

Before asking for review, check that:

- [ ] The branch builds and the relevant tests pass.
- [ ] New and changed C++ is formatted with `.clang-format`.
- [ ] Docs are updated for any behavior change.
- [ ] No generated files, local config, credentials or personal data are included.
- [ ] Any new third-party content is redistributable and recorded in the notices.
- [ ] `ProjectVersion` in `Config/DefaultGame.ini` is unchanged; maintainers handle releases.

Keep pull requests small enough to review in one sitting. Reply to every review comment,
either with a change or a reason, and push follow-up commits rather than force-pushing during review.

## Workflows and runners

Packaging runs only on the maintainer's self-hosted runners, started by hand; see
[Builds and releases](Docs/Releases.md). Pull requests don't get CI builds, so run your
checks locally.

If you change a workflow:

- Never add `pull_request` or `pull_request_target` triggers to a workflow that uses the
  self-hosted `ue-5.8` runners. A fork could run code on the maintainer's machine.
- Pin third-party actions to a full commit SHA.
- Give jobs the least permissions they need.
- Run [`actionlint`](https://github.com/rhysd/actionlint) on changed workflows. It reads
  `.github/actionlint.yaml`, which declares the custom `ue-5.8` runner label.

## Reporting bugs

Open an issue with:

- The game version (shown on the HUD, for example **v0.1.1**), or the commit if you built it.
- Your OS, GPU and driver version.
- How you were hitting shots: replay, a launch monitor over Bluetooth, or Open Connect.
- Steps to reproduce, what you expected and what happened.
- The log, from `Saved/Logs/` in the editor, `<game folder>/UOpenGolf/Saved/Logs/` in a
  packaged game, or `~/.config/Epic/UOpenGolf/Saved/` for the Ubuntu package.
- For a physics problem, the shot recording from `Documents/UOpenGolf/Saved/ShotRecordings`.

Logs contain local paths and your machine name. Remove anything you don't want to share.

## Security issues

Don't report security problems in a public issue. Use **Report a vulnerability** on the
repository's **Security** tab if it is available, or contact a maintainer listed in
`.github/CODEOWNERS` privately. Include the affected version and steps to reproduce.

## Licence

Everything you contribute is open source from the moment you submit it. That covers
code, scripts, tests, documentation, assets and their sources, maps, designs, data and
any other work. It is licensed under the repository's [MIT licence](LICENSE), with no
additional terms (*inbound = outbound*). In summary:

- You keep your copyright, and everyone gets the MIT permissions for your work. No
  contributor licence agreement or copyright assignment is required.
- Contributions are public, permanent and voluntary. They give no right to payment,
  royalties or control over the project.
- You confirm the work is yours to release under MIT, with any employer's permission it
  needs, and that any third-party material in it is redistributable and recorded in the notices.

The full terms are in [Licensing: Contributions](Docs/Licensing.md#contributions).
Unreal Engine and third-party assets keep their own licences.
