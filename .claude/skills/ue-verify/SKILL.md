---
name: ue-verify
description: Build the UOpenGolf Editor target and run the project smoke test, then report results honestly (what passed, what failed, what platforms/checks were not covered). Use after C++/config changes or when asked to verify the project.
---

Verify the project on this machine (Windows) using the project scripts, never the full engine test suite.

1. Build: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Build` (timeout ~600000 ms). On failure, read the tail of `Saved/Logs/BuildEditor.log`, find the first real compiler/UBT error, and report it with file:line. Stop here.
2. Smoke: `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/ue.ps1 Smoke` (timeout ~600000 ms). Smoke depends on step 1 succeeding.
3. Find the newest `Saved/Automation/<runid>/` directory. Summarize `index.json` (each test name, state, failed/notRun counts). On failure, show the relevant errors from `Unreal.log` / `stderr.log` in that directory.
4. If `$ARGUMENTS` names a specific test (e.g. `UOpenGolf.Smoke.HelloWorld`), also run it alone by invoking `UnrealEditor-Cmd.exe` with the same arguments `Scripts/ue.ps1` uses for Smoke but `-ExecCmds="Automation RunTests $ARGUMENTS"`.
5. If Python scripts under `Scripts/` changed, also run `python Scripts/test_automation_report.py`.

Report format:
- Build: pass/fail
- Smoke: pass/fail, with per-test results
- Not covered: always state that Linux was not run, and that smoke is headless (`-nullrhi`), so visual/presentation changes still need a rendered check.

If the verification evidence changes the platform status, point out that README.md's status/platform table may need updating. Don't edit README.md unless asked.
