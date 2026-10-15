# Repository guidance

## Current scope

uopengolf is an Unreal 5.8.3 C++ hello-world foundation. Windows and Linux are
intended supported platforms. Read README.md, Docs/Architecture.md, and
Docs/Roadmap.md before expanding the game. Roadmap phases are not instructions
to perform migrations or imports without an explicit task.

## Code and platform boundaries

- Runtime game code is C++; Blueprint wrappers can expose gameplay capabilities.
- `.Build.cs` and `.Target.cs` are standard UnrealBuildTool configuration, not C# gameplay.
- Keep platform-specific launch-monitor transports out of gameplay and physics.
- Keep future simulation calculations separate from Unreal scene queries and presentation.
- Keep filenames/references case-correct for Linux. Do not commit local engine paths.
- Sibling repositories are references only; do not edit or vendor them as part of routine game work.

## Documentation and licensing

- Update README.md alongside implemented changes, including its linked table of contents.
- Distinguish working features from planned features, and tested platforms from intended targets.
- Keep commands, prerequisites, known limitations, and roadmap status current.
- Original contributions use MIT. Preserve upstream notices and review licenses before imports.
- Never commit restricted content or derivatives without redistribution rights, including via Git LFS.
- `Plugins/PrivateContent/` is ignored and optional; the public project must work without it.

## Verification

- Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Build`, then `Smoke`.
- Linux: `bash Scripts/ue.sh build`, then `smoke`, with UE_ROOT set.
- Use the project smoke test, not the entire engine test suite.
- Headless automation verifies startup behavior; visually inspect presentation changes with rendering enabled.
- Report untested platforms honestly. Update validation notes when evidence changes.
