# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

## Git

- Git is read-only for Claude: never commit, push, pull, fetch, merge, rebase, reset, or check out. Inspection (`status`, `diff`, `log`, `show`) is fine. Ask the user to run any write operation.

## Commands

- Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 <Action>` with Action = `Build | Editor | Run | Smoke | ProjectFiles | CompileCommands`. Flags: `-EngineRoot PATH`, `-Target Editor|Game`, `-TimeoutSeconds N`.
- Linux: `bash Scripts/ue.sh <action>` with `--engine-root`, `--target`, `--timeout`; requires `UE_ROOT` (no fallback).
- Engine is at the Windows default `C:\Program Files\Epic Games\UE_5.8` (`UE_ROOT` not set); the script falls back to it.
- Always `Build` the Editor target before `Smoke`/`Editor`/`Run`. `-Target Game` only compiles; it does not cook or package.
- Smoke runs `Automation RunTests UOpenGolf.Smoke` headless (`-nullrhi`) and validates `Saved/Automation/<runid>/index.json` with `Scripts/check_automation_report.py`, which requires `UOpenGolf.Smoke.HelloWorld` to exist and pass. Logs: `Saved/Logs/`, `Saved/Automation/<runid>/`.
- There is no single-test filter flag; to run one test, invoke UnrealEditor-Cmd with `-ExecCmds="Automation RunTests <FullTestName>"` mirroring the Smoke invocation in `Scripts/ue.ps1`.
- Engine-free validator test: `python Scripts/test_automation_report.py` (use `python`, not `python3`, on this machine).
- `ProjectFiles` can fail because the engine install dir is write-protected; `CompileCommands` needs a prior successful editor build.

## Gotchas

- Keep `"EngineAssociation": "5.8"` in `UOpenGolf.uproject`; never change it to `5.8.3` or accept the editor's conversion prompt.
- Automation tests live in `Source/UOpenGolf/Private/Tests/`, are guarded by `#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`, and are named `UOpenGolf.<Category>.<Name>`.
- Source files start with `// SPDX-License-Identifier: MIT` (`#` in scripts). `.editorconfig`: LF, 4-space indent (2 for json/uproject/md), Unreal-style Allman braces with spaces, not tabs.
- Format changed C++ files with `"/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-format.exe" -i <file>` (not on PATH). `.clang-format` keeps `SortIncludes: Never` because `*.generated.h` must stay last.
- `.vscode/` is gitignored and local; don't treat it as project config.
- Don't add speculative plugins, public structs, or transports ahead of the roadmap phase that defines their contracts (see Docs/Architecture.md).
- `Content/**/*.umap|*.uasset` are binary; don't hand-edit them. `Scripts/CreateHelloWorldMap.py` is maintainer-only and refuses to overwrite.
