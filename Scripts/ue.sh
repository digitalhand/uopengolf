#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

usage() {
    echo "Usage: bash Scripts/ue.sh {build|editor|run|smoke|test|projectfiles|compilecommands} [--engine-root PATH] [--target editor|game] [--timeout SECONDS] [--filter TEST.PREFIX]"
}

action="${1:-build}"
if [[ $# -gt 0 ]]; then shift; fi
engine_root="${UE_ROOT:-}"
target=editor
timeout_seconds=180
filter=UOpenGolf
while [[ $# -gt 0 ]]; do
    case "$1" in
        --engine-root|--target|--timeout|--filter)
            [[ $# -ge 2 ]] || { usage; exit 2; }
            case "$1" in
                --engine-root) engine_root="$2" ;;
                --target) target="$2" ;;
                --timeout) timeout_seconds="$2" ;;
                --filter) filter="$2" ;;
            esac
            shift 2 ;;
        *) usage; exit 2 ;;
    esac
done
case "$action" in build|editor|run|smoke|test|projectfiles|compilecommands) ;; *) usage; exit 2 ;; esac
case "$target" in editor|game) ;; *) usage; exit 2 ;; esac
[[ "$timeout_seconds" =~ ^[1-9][0-9]*$ ]] && (( timeout_seconds <= 3600 )) || { echo 'Timeout must be 1..3600 seconds.' >&2; exit 2; }
[[ "$filter" =~ ^[A-Za-z0-9_.]+$ ]] || { echo 'Filter must be an automation test name prefix.' >&2; exit 2; }
[[ -n "$engine_root" ]] || { echo 'Set UE_ROOT or supply --engine-root pointing to Unreal Engine 5.8.' >&2; exit 2; }

script_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_root/.." && pwd)"
project="$project_root/UOpenGolf.uproject"
engine_root="$(cd -- "$engine_root" && pwd)"
editor="$engine_root/Engine/Binaries/Linux/UnrealEditor"
build="$engine_root/Engine/Build/BatchFiles/Linux/Build.sh"
[[ -x "$editor" && -f "$build" ]] || { echo "Unreal Linux tools not found in $engine_root" >&2; exit 2; }
mkdir -p "$project_root/Saved/Logs"

case "$action" in
    compilecommands)
        # Build first so UnrealHeaderTool's generated headers already exist.
        mkdir -p "$project_root/.vscode"
        bash "$build" UOpenGolfEditor Linux Development "-Project=$project" -Mode=GenerateClangDatabase \
            -NoExecCodeGenActions "-OutputDir=$project_root/.vscode" \
            '-OutputFilename=compileCommands_UOpenGolf.json' "-Log=$project_root/Saved/Logs/CompileDatabase.log"
        [[ -s "$project_root/.vscode/compileCommands_UOpenGolf.json" ]] || { echo 'Compilation database was not written.' >&2; exit 1; }
        ;;
    projectfiles)
        bash "$build" -projectfiles "-Project=$project" -game -engine -vscode -automated \
            "-Log=$project_root/Saved/Logs/GenerateVSCode.log"
        ;;
    build)
        build_target=UOpenGolfEditor
        [[ "$target" != game ]] || build_target=UOpenGolf
        bash "$build" "$build_target" Linux Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE \
            "-Log=$project_root/Saved/Logs/Build-$target.log"
        ;;
    editor) exec "$editor" "$project" ;;
    run) exec "$editor" "$project" -game -windowed -ResX=1280 -ResY=720 ;;
    smoke|test)
        # smoke runs the startup tests and requires UOpenGolf; test runs any prefix.
        tests=UOpenGolf.Smoke
        validation=()
        if [[ "$action" == test ]]; then tests="$filter"; validation=(--prefix "$filter"); fi
        command -v python3 >/dev/null || { echo 'python3 is required to validate smoke reports.' >&2; exit 2; }
        command -v timeout >/dev/null || { echo 'GNU coreutils timeout is required.' >&2; exit 2; }
        mkdir -p "$project_root/Saved/Automation"
        report="$(mktemp -d "$project_root/Saved/Automation/$(date +%Y%m%d-%H%M%S)-XXXXXX")"
        echo "Running $tests automation tests; report: $report"
        # Linux uses UnrealEditor with unattended/NullRHI flags; no separate -Cmd binary is required.
        if timeout --kill-after=10s "${timeout_seconds}s" "$editor" "$project" \
            -unattended -nopause -nosplash -nosound -nullrhi -DDC=UOpenGolfSmoke -stdout -FullStdOutLogOutput \
            "-ExecCmds=Automation RunTests $tests" '-TestExit=Automation Test Queue Empty' \
            "-ReportExportPath=$report" "-abslog=$report/Unreal.log" \
            >"$report/stdout.log" 2>"$report/stderr.log"; then
            python3 "$script_root/check_automation_report.py" "$report/index.json" ${validation[@]+"${validation[@]}"}
        else
            result=$?
            echo "Automation process failed or timed out (exit $result). See $report" >&2
            exit "$result"
        fi
        ;;
esac
