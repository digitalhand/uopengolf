# Unreal Open Golf 

An open source golf game built with Unreal Engine and native C++, intended for
Windows and Linux. Original project code, documentation, and original assets use
the [MIT license](LICENSE). Unreal Engine and third-party content retain their own licenses.

## Table of contents

- [Current status](#current-status)
- [Platforms](#platforms)
- [Prerequisites](#prerequisites)
- [Getting started](#getting-started)
- [Engine version warning](#engine-version-warning)
- [Visual Studio Code setup](#visual-studio-code-setup)
- [Build, run, and test](#build-run-and-test)
- [Project structure and architecture](#project-structure-and-architecture)
- [Launch monitors and planned integrations](#launch-monitors-and-planned-integrations)
  - [Testing launch-monitor capture](#testing-launch-monitor-capture)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [Licensing and third-party assets](#licensing-and-third-party-assets)

## Current status

The startup map is blank; Play displays **Hello, Unreal Open Golf** and a
launch-monitor panel through a C++ HUD. There are no golf mechanics, physics, or
course yet.

Working on Windows (Unreal 5.8.3, verified 2026-10-14 by headless automation):

- Editor and game targets build; `UOpenGolf.Smoke.HelloWorld` passes.
- The `OpenLaunchMonitor` plugin: a native C++ port of the open-lm-connector Square
  protocol and session, a C++/WinRT Bluetooth LE transport, built-in replay shots,
  and Blueprint/C++ events. Its protocol, session and replay tests pass.

Verified by the maintainer with a **real Square Home** on Windows (2026-10-14): the
Bluetooth connection, battery and firmware readout, and captured shot and club data.

Not yet verified: link-loss reconnect and club selection on hardware, and anything
on Linux. Linux has no Bluetooth transport yet.

No OpenFairway or ogolf code has been imported. Launch-monitor hardware and
purchased content are optional; replay shots work without hardware.

## Platforms

| Platform | Intended support | Verification |
| --- | --- | --- |
| Windows x64 | Yes, from the foundation onward | Editor/game build, smoke and launch-monitor tests pass; Square Home connection, shot and club data verified |
| Linux x86_64 | Yes, deferred track | Build/run/test scripts provided; not yet executed on Linux; no Bluetooth transport yet |

Keep paths case-correct and platform-specific integrations outside shared gameplay.
Linux support is a project goal; it does not change Unreal Engine's license.

## Prerequisites

### Windows

- Unreal Engine **5.8.3**, including its C++ development components.
- Visual Studio 2022 **17.14 or later** with Game development with C++, MSVC v143,
  and a Windows SDK compatible with Unreal 5.8. The initial machine has MSVC
  14.44.35207 and Windows SDK 10.0.26100.0.
- Windows PowerShell 5.1 or PowerShell 7; smoke report checks use Unreal's bundled Python.
- The Windows SDK's C++/WinRT headers (`Include/<version>/cppwinrt`), installed with
  the Windows SDK component above; the launch-monitor plugin uses them.
- Optional, for a real launch monitor: Windows 10/11 with Bluetooth LE turned on and a
  Square Home. No pairing or extra permissions are needed.
- A **.NET Framework SDK** (4.6 or newer is required by the engine's Swarm module;
  install the 4.8 SDK/developer tools through Visual Studio Installer). A modern
  .NET SDK/runtime alone does not supply this Windows editor build dependency.

See Epic's [Visual Studio setup guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine).
If the build says `Could not find NetFxSDK install dir`, add the .NET Framework SDK
component and retry. This component was not found on the initial machine; the
system-wide Visual Studio installation has not been modified by this project.

### Linux

- A Linux Unreal Engine **5.8.3** installation or source build, with the engine's
  supported Clang toolchain and required system dependencies. Obtain the engine separately.
- Bash, Python 3, and GNU coreutils (`timeout`, `mktemp`).
- A supported graphics driver and desktop session for the editor and visual verification.
  The smoke command uses `-nullrhi` and does not test rendering.

Follow Epic's [Linux development requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine)
for the engine version being used. Linux prerequisites have not been validated locally.

Bluetooth and BlueZ are **not needed to build**. A future Linux Square transport will
require BlueZ and appropriate Bluetooth access; it is not implemented yet.

## Getting started

Obtain Unreal Engine separately, clone this repository, and set `UE_ROOT` to the
directory containing `Engine/`. The project requires no sibling repositories.

Windows PowerShell:

```powershell
$env:UE_ROOT = 'C:\Program Files\Epic Games\UE_5.8'
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Build
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Editor
```

Linux Bash (commands supplied for future Linux validation):

```bash
export UE_ROOT='/path/to/UnrealEngine-5.8.3'
bash Scripts/ue.sh build
bash Scripts/ue.sh editor
```

Press **Play** in the editor. The default `HelloWorld` map should display the greeting
in white against a blank background, with the launch-monitor panel at the top left.
Stop Play using the editor toolbar or Escape. See
[Testing launch-monitor capture](#testing-launch-monitor-capture) for the panel's keys.

## Engine version warning

The project uses `"EngineAssociation": "5.8"`; this is the normal release association
for Unreal **5.8.3**, not a request to downgrade to 5.8.0. The original map was also
generated with 5.8.3. Do not change the association to `5.8.3` merely to match the
patch number: it identifies a registered engine installation.

Unreal's project browser shows "This project was made with a different version"
when the project's engine identifier differs from the running editor's identifier.
That can happen with matching engine versions if an installation is unregistered
or registered under a custom identifier. During initial troubleshooting, this
machine's launcher installation list did not contain the installed `UE_5.8` directory.

Select the intended engine explicitly and launch directly after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Build -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Editor -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

This bypasses the project browser's identifier-based conversion prompt. If already
in that browser, first confirm **Help > About Unreal Editor** reports 5.8.3; then
**More Options > Skip conversion** avoids changing the project for this association
mismatch. For persistent Explorer/browser association, use the `.uproject` file's
**Switch Unreal Engine version** action, if available, and select the actual 5.8
installation. Keep machine-specific identifiers out of shared project changes.

Opening still requires a successful C++ build. A missing-module or NetFxSDK error
is a separate build prerequisite problem; skipping conversion does not fix it.

## Visual Studio Code setup

Install Microsoft's **C/C++** extension (`ms-vscode.cpptools`) and open the actual
`uopengolf` repository folder in VS Code. A generated conversion copy such as
`uopengolf 5.8 - 2` has separate source and configuration files.

Generate the native Unreal VS Code workspace and IntelliSense configuration:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 ProjectFiles
```

On Linux, set `UE_ROOT` and run `bash Scripts/ue.sh projectfiles` instead. This
uses UnrealBuildTool's VS Code generator, following
[Epic's VS Code setup workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-code-for-unreal-engine).
Open the resulting `UOpenGolf.code-workspace` file. Generation creates local
`.vscode/c_cpp_properties.json`, build/debug configuration and compile-command
data with paths for the machine generating them. Regenerate after changing source
files, engine/toolchain installations, or platforms. Build the editor target to
produce UnrealHeaderTool's generated headers needed by IntelliSense.

A local Windows starter configuration has also been created in this checkout:

- `.vscode/c_cpp_properties.json`: installed MSVC path and fallback Unreal/project include paths.
- `.vscode/tasks.json`: **Ctrl+Shift+B** builds the editor; **Tasks: Run Task** offers
  smoke testing and native VS Code project generation.
- `.vscode/launch.json`: **F5** builds and starts Unreal Editor under the Windows debugger.
- `.vscode/extensions.json`: recommends the Microsoft C/C++ extension.

The local C/C++ configuration now points to a real UnrealBuildTool-generated
`.vscode/compileCommands_UOpenGolf.json`. It contains compiler invocations for the
project's translation units, including generated ones (regenerate after adding files). Each command references Unreal-generated
response files containing target-specific include paths, definitions and options.
The recursive project include paths remain as a fallback for files without an entry.
Do not copy `MyProject` or compiler paths from tutorial screenshots.

To refresh just the compilation database after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 CompileCommands
```

The local VS Code task **uopengolf: refresh compile commands** runs the same command.
On Linux, use `bash Scripts/ue.sh compilecommands` (not yet executed on Linux).
This uses UnrealBuildTool's `GenerateClangDatabase` mode with the Windows MSVC
toolchain explicitly selected; installing Clang is not necessary for this Windows
workflow. The generator disables unity builds/PCHs to produce per-file entries.
Generated headers must already exist from a successful editor build because this
command skips executing code-generation actions. It does not compile the game.

Keep `Intermediate/` alongside the database: its referenced response files,
definitions and generated headers are needed by IntelliSense. Regenerate after
cleaning build output or changing source files/toolchains. If VS Code does not
refresh, run **C/C++: Reset IntelliSense Database**, then reopen a project C++ file.

`.vscode/` and `*.code-workspace` remain ignored because these files contain local
compiler/engine paths. They are not distributed with a fresh clone; generate them
with the commands above. Do not force-add them to the repository.

Agent-side native generation was attempted but blocked when Unreal tried to write
`Engine/Intermediate/ProjectFiles/PrimaryProjectPath.txt` inside its protected
installation. Generating just the compilation database succeeded separately and
its source/compiler/response-file paths were checked. Full native workspace
generation and interactive VS Code IntelliSense/debugging remain unverified here.

## Build, run, and test

Run commands from the repository root. Windows scripts also accept `-EngineRoot PATH`;
Linux scripts accept `--engine-root PATH`. Windows falls back to the standard
`Program Files/Epic Games/UE_5.8` location when `UE_ROOT` is unset.

| Action | Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File` | Linux: `bash` |
| --- | --- | --- |
| Build Development Editor | `Scripts/ue.ps1 Build` | `Scripts/ue.sh build` |
| Open editor | `Scripts/ue.ps1 Editor` | `Scripts/ue.sh editor` |
| Run through editor executable in game mode | `Scripts/ue.ps1 Run` | `Scripts/ue.sh run` |
| Run headless startup smoke test | `Scripts/ue.ps1 Smoke` | `Scripts/ue.sh smoke` |
| Run automation tests by name prefix | `Scripts/ue.ps1 Test -Filter UOpenGolf.LaunchMonitor` | `Scripts/ue.sh test --filter UOpenGolf.LaunchMonitor` |
| Compile Development game target | `Scripts/ue.ps1 Build -Target Game` | `Scripts/ue.sh build --target game` |

Build the editor target **before** running the editor, game-mode preview, or smoke test.
The game-target command compiles native code; it does not cook or package a standalone game.
Packaging automation is a later roadmap milestone.

Unreal's command-line equivalents to Godot console/headless workflows are
`Build.bat` / `Build.sh` (UnrealBuildTool), `UnrealEditor-Cmd.exe` on Windows or
`UnrealEditor` on Linux with unattended options, and the Automation Framework.
Future packaging will use `RunUAT.bat` / `RunUAT.sh` with `BuildCookRun`.

The `UOpenGolf.Smoke.HelloWorld` test loads the committed map, starts Play In Editor,
and verifies the map, game mode, spawned HUD, and greeting value. It does not
verify pixels; visually inspect the greeting with rendering enabled as well.

`Test` runs every automation test whose name starts with `-Filter` (default
`UOpenGolf`, which includes the smoke test) and fails unless at least one matching
test ran and every test passed. The launch-monitor tests are
`UOpenGolf.LaunchMonitor.Protocol`, `.Session` and `.Replay`; they need no hardware.

Each smoke or test run writes logs and an `index.json` report to a fresh directory under
`Saved/Automation/`. The runner fails on a nonzero process exit, timeout, missing or
invalid report, zero tests, a missing HelloWorld test, or failed/incomplete tests.
Smoke runs select a project-local filesystem cache in `DerivedDataCache/`, without
requiring a Zen cache service; normal interactive editor sessions use engine defaults.
Default timeout is 180 seconds; use `-TimeoutSeconds 300` or `--timeout 300` on slower machines.
Build logs are under `Saved/Logs/`. Generated outputs are ignored by Git.

The report validator also has engine-free checks:
`python Scripts/test_automation_report.py` (Windows) or
`python3 Scripts/test_automation_report.py` (Linux).

The original blank map is committed. `Scripts/CreateHelloWorldMap.py` is a maintainer
generation utility, not a setup step; it refuses to replace an existing map.

## Project structure and architecture

```text
Config/                   Startup map, game mode, and project settings
Content/UOpenGolf/Maps/    Original HelloWorld map
Source/UOpenGolf/          Game module: GameMode, HUD panel, test controls, smoke test
Plugins/OpenLaunchMonitor/ Launch-monitor plugin: Square protocol, Bluetooth, replay, events
Scripts/                  Build/run/smoke commands and report validation
Docs/                     Architecture, roadmap, phase status, and licensing policy
```

Runtime behavior is **C++**, with Unreal reflection where useful for Blueprints.
The `.Build.cs` and `.Target.cs` files are standard **C# UnrealBuildTool configuration**;
they describe compilation and targets, not a C# gameplay runtime. The small Python
utilities support map generation and test report validation only.

See [Architecture](Docs/Architecture.md) for future component boundaries and data flow.

## Launch monitors and planned integrations

| Reference | Planned role | Current integration |
| --- | --- | --- |
| `open-lm-connector` | Launch-monitor connection, initially **Square Home** | Protocol, session and Windows Bluetooth ported to C++ in `Plugins/OpenLaunchMonitor` (revision `92eb021`) |
| `openfairway` | C# physics to migrate to native C++, exposed through Unreal and Blueprints | None |
| `ogolf` | Reference for greens, lies, and course/surface usage | None |

The local reference repositories are siblings under
`C:\Users\futco\Code\Github\digitalhand` on the initial development machine.
This is a maintainer reference location, not a build dependency or portable configuration.

Unreal cannot load the connector's Godot addon, so its Godot-free C# parts were
ported to native C++ (no .NET runtime). Units, signs, missing values, threading and
connection lifecycle are documented in
[launch-monitor integration](Docs/LaunchMonitorIntegration.md). Replay shots use the
same subsystem events as the device, so contributors can test without hardware.

Blueprint/C++ access: `UOpenLaunchMonitorSubsystem` (from the game instance) has
`Connect`, `Disconnect`, `SetClub`, `SetLeftHanded`, `InjectReplayShot` and
`InjectReplayFrames`, getters for the panel's values, and events `OnConnectionStateChanged`,
`OnBallReadyChanged`, `OnBatteryChanged`, `OnShotReceived`, `OnClubDataReceived` and
`OnError`. Shots carry device units (m/s, degrees, rpm) with GSPro signs and a
Measured/Derived/Missing source for each value.

### Testing launch-monitor capture

1. Build, open the editor, and press **Play**. Click in the viewport so it has input focus.
2. Press **R** to inject a replay shot. The panel shows shot #1, a 114.5 mph replay
   (VLA 6.95°, HLA -0.63°, 1933 rpm, axis -1.42°) with club path/face/attack/loft.
   Each press cycles through three built-in shots (a putt included).
3. With a Square Home powered on and Windows Bluetooth enabled, press **C**. The state
   goes Scanning → Connecting → Connected → Ready (about 5 s after connecting), with
   device name, battery and firmware. Place a ball; the panel shows when it is ready.
4. Hit a shot. The panel shows the measured values; club data follows within about
   a second ("not tracked" without a club sticker). The device re-arms after 3 s.
5. **Tab** selects the next club (sent to the device); **X** disconnects.

Values marked `*` are derived, and `—` means the device did not measure it.
Details are logged under `LogOpenLaunchMonitor` (Output Log or `Saved/Logs/`).

Known limitations: Windows only; the first matching device found is used; scans stop
after 30 s; automatic reconnect tries five times after a dropped link; Square Omni,
alignment and spin-mode selection are not exposed in the UI; reconnect after link
loss has not been tested on hardware.

## Roadmap

Planned work and completion criteria are in the [roadmap](Docs/Roadmap.md);
milestone progress is tracked in [phase status](Docs/PhasesStatus.md). Planned
milestones are not implemented features or implicit migration tasks.

## Contributing

- Read [AGENTS.md](AGENTS.md), [Architecture](Docs/Architecture.md), and the roadmap.
- Update this README **as features change**, including its table of contents, commands,
  platform verification status, and limitations. Link detailed designs from here.
- Run the relevant project smoke test; report the engine version and OS actually tested.
- Format C++ with the repository [.clang-format](.clang-format) (Unreal-style, never reorders
  includes). Visual Studio 2022 ships `clang-format` under `VC\Tools\Llvm\x64\bin`.
- Keep public builds usable without launch monitors, purchased packs, or local sibling repositories.
- Contribute only content you have permission to release.
- Record code or assets from anyone other than the maintainer in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licensing and third-party assets

Original uopengolf code, documentation, and original assets are licensed under
[MIT](LICENSE). Unreal Engine is separately licensed. See
[Licensing](Docs/Licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

Purchased or otherwise restricted assets belong in separately maintained private
content packs under the ignored `Plugins/PrivateContent/` directory. Public code,
maps, and defaults must not depend on them. Adding an MIT license to this repository
does not grant redistribution rights to purchased assets or their derivatives.
