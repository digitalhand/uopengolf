# Building and testing

## Prerequisites

### Windows

- Unreal Engine **5.8.4**, including its C++ development components.
- Visual Studio 2022 **17.14 or later** with Game development with C++, MSVC v143,
  and a Windows SDK compatible with Unreal 5.8.
- Git with Git LFS. Git for Windows includes it; run `git lfs install` once.
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
component and retry.

### Linux

- A Linux Unreal Engine **5.8.4** installation or source build, with the engine's
  supported Clang toolchain and required system dependencies. Obtain the engine separately.
- Bash, Python 3, and GNU coreutils (`timeout`, `mktemp`).
- Git with Git LFS (for example `sudo apt install git-lfs`); run `git lfs install` once.
- A supported graphics driver and desktop session for the editor and visual verification.
  The smoke command uses `-nullrhi` and does not test rendering.

Follow Epic's [Linux development requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine)
for the engine version being used. The project targets 5.8.4; see phase status for dated platform evidence.

Bluetooth and BlueZ are **not needed to build**. Connecting a Square Home on Linux needs a
running BlueZ (`bluetoothd`) and a user allowed to use it; `libsystemd.so.0` is loaded at runtime.

### Graphics hardware

The game and editor render with **Shader Model 6 only**: DirectX 12 on Windows and Vulkan on
Linux. There is no DirectX 11 or SM5 fallback, because the range's Nanite foliage and virtual
shadow maps need SM6 (`Config/DefaultEngine.ini`). The GPU and driver must expose DirectX 12
feature level 12_1 with Shader Model 6.6 and its 64-bit atomics (on Linux, Vulkan with
`VK_KHR_shader_atomic_int64`). Windows 10 needs version 1909 or later for the DirectX 12 Agility SDK.

Check the required features with DirectX Caps Viewer on Windows or `vulkaninfo` on Linux.
See Epic's [rendering requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-and-software-specifications-for-unreal-engine).

## Developer

Building, packaging, testing and editor setup for working on the project.

### Build, run, and test

Run commands from the repository root. Windows scripts also accept `-EngineRoot PATH`;
Linux scripts accept `--engine-root PATH`. Windows falls back to the standard
`Program Files/Epic Games/UE_5.8` location when `UE_ROOT` is unset.

| Action | Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File` | Linux: `bash` |
| --- | --- | --- |
| Build Development Editor | `Assets/Scripts/ue.ps1 Build` | `Assets/Scripts/ue.sh build` |
| Open editor | `Assets/Scripts/ue.ps1 Editor` | `Assets/Scripts/ue.sh editor` |
| Run through editor executable in game mode | `Assets/Scripts/ue.ps1 Run` | `Assets/Scripts/ue.sh run` |
| Run headless startup smoke test | `Assets/Scripts/ue.ps1 Smoke` | `Assets/Scripts/ue.sh smoke` |
| Run automation tests by name prefix | `Assets/Scripts/ue.ps1 Test -Filter UOpenGolf.LaunchMonitor` | `Assets/Scripts/ue.sh test --filter UOpenGolf.LaunchMonitor` |
| Compile Development game target | `Assets/Scripts/ue.ps1 Build -Target Game` | `Assets/Scripts/ue.sh build --target game` |
| Package the game and check it starts | `Assets/Scripts/ue.ps1 Package` | `Assets/Scripts/ue.sh package` |

Build the editor target **before** running the editor, game-mode preview, or smoke test.
The game-target command compiles native code; it does not cook or package a standalone
game. To build a standalone game, see [Package (Windows and Linux)](#package-windows-and-linux).

Unreal's command-line equivalents to Godot console/headless workflows are
`Build.bat` / `Build.sh` (UnrealBuildTool), `UnrealEditor-Cmd.exe` on Windows or
`UnrealEditor` on Linux with unattended options, the Automation Framework, and
`RunUAT.bat` / `RunUAT.sh` with `BuildCookRun` for packaging.

The `UOpenGolf.Smoke.UOpenGolf` startup test (named after the committed map) loads
the map, starts Play In Editor, and verifies the game mode, HUD, range, and a ball
resting on the tee. It does not verify pixels; check the range visually with
rendering enabled as well.

`Test` runs every automation test whose name starts with `-Filter` (default
`UOpenGolf`, which includes the smoke test) and fails unless at least one matching
test ran and every test passed. See [Testing](Testing.md) for headless, rendered and hardware checks.

Each smoke or test run writes logs and an `index.json` report to a fresh directory under
`Saved/Automation/`. The runner checks the exit code and report, and fails on timeout or a missing or
invalid report, zero tests, a missing UOpenGolf test, or failed/incomplete tests.
Smoke runs select a project-local filesystem cache in `DerivedDataCache/`, without
requiring a Zen cache service; normal interactive editor sessions use engine defaults.
Default timeout is 600 seconds; use `-TimeoutSeconds` or `--timeout` (up to 3600) to change it.
Build logs are under `Saved/Logs/`. Generated outputs are ignored by Git.

The report validator and the FlightScope comparison script also have engine-free checks:
`python3 -m unittest Assets.Scripts.test_automation_report` and `python3 -m unittest Assets.Scripts.test_flightscope_compare` from the repository root
(Windows), or the same with `python3` (Linux).

The public range map, `/Game/UOpenGolf/Maps/UOpenGolf`, is the project's only map and is
committed with its terrain, vegetation and range actors.

### Package (Windows and Linux)

Packaging builds the game target, cooks content, and stages a standalone game that runs
without the editor. Close the Unreal Editor first, then run from the repository root:

| | Windows | Linux |
| --- | --- | --- |
| Command | `powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Package` | `bash Assets/Scripts/ue.sh package` |
| Output | `Packaged/Windows/` | `Packaged/Linux/` |
| Run the game | `Packaged/Windows/UOpenGolf.exe` | `Packaged/Linux/UOpenGolf.sh` |
| Game logs | `Packaged/Windows/UOpenGolf/Saved/Logs/` | `Packaged/Linux/UOpenGolf/Saved/Logs/` |
| Verified | Manual `RunUAT` package, 2026-10-15 (about 1 GB); `Package` action not yet run | `package`, 2026-10-18 (1.1 GB, about 2 minutes with a warm cache) |

The action deletes the previous `Packaged/<platform>/` folder, runs `RunUAT BuildCookRun`
for a Development package (log: `Saved/Logs/Package.log`), then starts the packaged game
headless for 60 s with its log in a new `Saved/Automation/<stamp>-package-*/Game.log`.
`Assets/Scripts/check_package_start.py` fails the run unless that log shows the startup map
loaded and contains no `Fatal error`, `Assertion failed`, `Unhandled Exception` or
`appError called`. The script takes the same `--engine-root` / `-EngineRoot` as the
other actions. `Packaged/` is ignored by Git. To share or move the game, copy the whole
platform folder, not just the executable. To build versioned release archives on GitHub
instead, see [CI and releases](Releases.md).

The underlying command, if you need different options (Windows shown; on Linux use
`Engine/Build/BatchFiles/RunUAT.sh` and `-platform=Linux`):

```powershell
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
& "$ue\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$PWD\UOpenGolf.uproject" `
  -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive `
  "-archivedirectory=$PWD\Packaged" -nop4 -utf8output -unattended
```

From the editor: **Platforms > Windows** (or **Linux**) **> Package Project**, then
choose the `Packaged` folder.

| Setting | Where | Purpose |
| --- | --- | --- |
| Map to cook | `Config/DefaultGame.ini`, `[/Script/UnrealEd.ProjectPackagingSettings]` `+MapsToCook` | Cooks `/Game/UOpenGolf/Maps/UOpenGolf` |
| Startup map and game mode | `Config/DefaultEngine.ini`, `[/Script/EngineSettings.GameMapsSettings]` | `GameDefaultMap` and `GlobalDefaultGameMode` used by the packaged game |
| Project name and version | `Config/DefaultGame.ini`, `[/Script/EngineSettings.GeneralProjectSettings]` | Package metadata and the HUD version label (`ProjectVersion=0.0.6`, shown as **v0.0.6**); CI release builds stamp it from the tag ([Versioning](Releases.md#versioning)) |
| Game target | `Source/UOpenGolf.Target.cs` | The `UOpenGolf` game target that `-build` compiles |
| Build configuration | `-clientconfig=` | `Development` is verified; `Shipping` has not been tried |
| Cook order logs | `Build/<platform>/FileOpenOrder/` | Written by packaging; ignored by Git |

In the packaged game, **Escape** quits. Shot recordings still go to
`Documents/UOpenGolf/Saved/ShotRecordings`, and a Square Home connects the same way as
in the editor (Linux needs `bluetoothd`; see the plugin README).

Not yet verified: connecting a Square Home from a packaged build (either platform), and
Shipping builds. The start check proves the game starts and loads the map headless; it
does not render or connect hardware.

### Engine version warning

The project uses `"EngineAssociation": "5.8"`; this is the normal release association
for Unreal **5.8.4**, not a request to downgrade to 5.8.0. The original map was
generated with 5.8.3. Do not change the association to `5.8.4` merely to match the
patch number: it identifies a registered engine installation. Patch updates within 5.8
(for example 5.8.3 to 5.8.4) keep the same association; the package check requires the
exact patch named in `Assets/Scripts/check_package_runner.py`.

Unreal's project browser shows "This project was made with a different version"
when the project's engine identifier differs from the running editor's identifier.
That can happen with matching engine versions if an installation is unregistered
or registered under a custom identifier.

Select the intended engine explicitly and launch directly after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Build -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 Editor -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

This bypasses the project browser's identifier-based conversion prompt. If already
in that browser, first confirm **Help > About Unreal Editor** reports 5.8.4; then
**More Options > Skip conversion** avoids changing the project for this association
mismatch. For persistent Explorer/browser association, use the `.uproject` file's
**Switch Unreal Engine version** action, if available, and select the actual 5.8
installation. Keep machine-specific identifiers out of shared project changes.

Opening still requires a successful C++ build. A missing-module or NetFxSDK error
is a separate build prerequisite problem; skipping conversion does not fix it.

### Visual Studio Code setup

Install Microsoft's **C/C++** extension (`ms-vscode.cpptools`) and open the actual
`uopengolf` repository folder in VS Code. A generated conversion copy such as
`uopengolf 5.8 - 2` has separate source and configuration files.

Generate the native Unreal VS Code workspace and IntelliSense configuration:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 ProjectFiles
```

On Linux, set `UE_ROOT` and run `bash Assets/Scripts/ue.sh projectfiles` instead. This
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
powershell -NoProfile -ExecutionPolicy Bypass -File Assets/Scripts/ue.ps1 CompileCommands
```

The local VS Code task **uopengolf: refresh compile commands** runs the same command.
On Linux, use `bash Assets/Scripts/ue.sh compilecommands`. Run it again after
`projectfiles`, which replaces the database with a smaller one.
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

On Linux (2026-10-16), `projectfiles` and `compilecommands` both succeed when the engine
directory is writable. The generated workspace has `cppdbg` launch entries that use gdb;
the **DebugGame** editor keeps project modules unoptimized for stepping. To display
`FString`, `TArray` and `FName` readably, add a gdb setup command that runs
`python import sys, gdb.printing; sys.path.insert(0, '<UE_ROOT>/Engine/Extras/GDBPrinters'); from UEPrinters import register_ue_printers; register_ue_printers(None)`
(the `gdb.printing` import is required). Folder-level `.vscode/tasks.json` and
`.vscode/launch.json` are not overwritten by regeneration, unlike the workspace file.
Generation also writes a local `.ignore` that excludes `Docs/`, `Assets/`, `Content/` and
`Data/` from ripgrep-based search.
