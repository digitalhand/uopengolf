# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('Build', 'Editor', 'Run', 'Smoke', 'Test', 'ProjectFiles', 'CompileCommands')]
    [string]$Action = 'Build',
    [string]$EngineRoot = $env:UE_ROOT,
    [ValidateSet('Editor', 'Game')]
    [string]$Target = 'Editor',
    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 180,
    # Test only: automation test name prefix to run, such as UOpenGolf.LaunchMonitor.
    [ValidatePattern('^[A-Za-z0-9_.]+$')]
    [string]$Filter = 'UOpenGolf'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'UOpenGolf.uproject'

try {
    if (-not $EngineRoot) {
        $EngineRoot = Join-Path $env:ProgramFiles 'Epic Games\UE_5.8'
    }
    $EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
    $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $commandEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    foreach ($tool in @($editor, $commandEditor, $build)) {
        if (-not (Test-Path -LiteralPath $tool)) { throw "Unreal tool not found: $tool" }
    }
    $logs = Join-Path $projectRoot 'Saved\Logs'
    New-Item -ItemType Directory -Path $logs -Force | Out-Null

    switch ($Action) {
        'CompileCommands' {
            $output = Join-Path $projectRoot '.vscode'
            New-Item -ItemType Directory -Path $output -Force | Out-Null
            # Use the installed MSVC toolchain; UBT otherwise defaults this mode to Clang.
            # Build first: generated headers are reused rather than executing codegen actions here.
            & $build UOpenGolfEditor Win64 Development "-Project=$project" -Mode=GenerateClangDatabase `
                -Compiler=VisualStudio2022 -NoExecCodeGenActions "-OutputDir=$output" `
                '-OutputFilename=compileCommands_UOpenGolf.json' "-Log=$logs\CompileDatabase.log"
            if ($LASTEXITCODE -ne 0) { throw "Compilation database generation failed (exit $LASTEXITCODE). See $logs\CompileDatabase.log" }
            $entries = @(Get-Content -LiteralPath (Join-Path $output 'compileCommands_UOpenGolf.json') -Raw | ConvertFrom-Json)
            if ($entries.Count -eq 0) { throw 'Unreal generated an empty compilation database.' }
            Write-Host "Generated $($entries.Count) compile commands in $output\compileCommands_UOpenGolf.json"
        }
        'ProjectFiles' {
            & $build -projectfiles "-Project=$project" -game -engine -vscode -automated "-Log=$logs\GenerateVSCode.log"
            if ($LASTEXITCODE -ne 0) { throw "VS Code project generation failed (exit $LASTEXITCODE). See $logs\GenerateVSCode.log" }
        }
        'Build' {
            $buildTarget = if ($Target -eq 'Editor') { 'UOpenGolfEditor' } else { 'UOpenGolf' }
            & $build $buildTarget Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE "-Log=$logs\Build$Target.log"
            if ($LASTEXITCODE -ne 0) { throw "Unreal build failed (exit $LASTEXITCODE)." }
        }
        'Editor' { & $editor $project }
        'Run' { & $editor $project -game -windowed -ResX=1280 -ResY=720 }
        { $_ -in 'Smoke', 'Test' } {
            # Smoke runs the startup tests and requires HelloWorld; Test runs any prefix.
            $tests = if ($Action -eq 'Smoke') { 'UOpenGolf.Smoke' } else { $Filter }
            $python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
            if (-not (Test-Path -LiteralPath $python)) { throw "Bundled Python not found: $python" }
            $runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
            $report = Join-Path $projectRoot "Saved\Automation\$runId"
            New-Item -ItemType Directory -Path $report -Force | Out-Null
            $arguments = @(
                $project, '-unattended', '-nopause', '-nosplash', '-nosound', '-nullrhi', '-DDC=UOpenGolfSmoke',
                '-stdout', '-FullStdOutLogOutput', "-ExecCmds=Automation RunTests $tests",
                '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$report", "-abslog=$report\Unreal.log"
            )
            # Start-Process joins its argument array; quote each argument for paths with spaces.
            foreach ($argument in $arguments) {
                if ($argument.Contains('"')) { throw 'Double quotes are not supported in engine/project paths.' }
            }
            $argumentLine = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
            $process = Start-Process -FilePath $commandEditor -ArgumentList $argumentLine -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput "$report\stdout.log" -RedirectStandardError "$report\stderr.log"
            # Retain the handle so ExitCode remains available after a short-lived process exits.
            $processHandle = $process.Handle
            Write-Host "Running $tests automation tests; report: $report"
            if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
                # Terminate Unreal and any worker processes belonging to this run.
                & taskkill.exe /PID $process.Id /T /F | Out-Null
                if ($LASTEXITCODE -ne 0 -and -not $process.HasExited) {
                    # Some restricted environments deny taskkill's process-tree query.
                    $process.Kill()
                }
                throw "Automation run exceeded $TimeoutSeconds seconds. See $report"
            }
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) { throw "Unreal automation process failed (exit $($process.ExitCode)). See $report" }
            $validation = @((Join-Path $PSScriptRoot 'check_automation_report.py'), (Join-Path $report 'index.json'))
            if ($Action -eq 'Test') { $validation += @('--prefix', $Filter) }
            & $python @validation
            if ($LASTEXITCODE -ne 0) { throw "Automation report validation failed. See $report" }
        }
    }
}
catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
