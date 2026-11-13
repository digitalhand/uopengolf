# SPDX-License-Identifier: MIT
# Builds UOpenGolf-<version>-Windows-x64-Setup.exe from a packaged game folder with Inno Setup 6.
# Used by the Package Windows workflow; locally, after `ue.ps1 Package`:
#   powershell -File Assets\Installer\build_installer.ps1 -PackageDir Packaged\Windows -Version 0.0.8 -OutputDir Packaged\dist
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$OutputDir,
    # ISCC.exe; found under Program Files or the per-user install (winget install JRSoftware.InnoSetup) when omitted.
    [string]$Iscc
)

$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)(-[0-9A-Za-z.-]+)?$') { throw "Version $Version is not MAJOR.MINOR.PATCH[-prerelease]." }
$numeric = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0"

if (-not $Iscc) {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
    )
    $Iscc = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
    if (-not $Iscc) { $Iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source }
}
if (-not $Iscc -or -not (Test-Path -LiteralPath $Iscc)) { throw 'Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup' }

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $PackageDir 'UOpenGolf.exe'))) { throw "No UOpenGolf.exe in $PackageDir." }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path
$name = "UOpenGolf-$Version-Windows-x64-Setup"

& $Iscc /Qp "/DAppVersion=$Version" "/DAppNumericVersion=$numeric" "/DSourceDir=$PackageDir" `
    "/DOutputDir=$OutputDir" "/DOutputBaseFilename=$name" (Join-Path $PSScriptRoot 'UOpenGolf.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed (exit $LASTEXITCODE)." }
Write-Host "Built $(Join-Path $OutputDir "$name.exe")"
