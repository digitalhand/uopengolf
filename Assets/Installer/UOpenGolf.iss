; SPDX-License-Identifier: MIT
; Windows installer for a packaged UOpenGolf build. The same Setup installs, upgrades in place and, run by the game
; with /SILENT, applies an in-game update. Build it with Assets/Installer/build_installer.ps1, which passes:
;   AppVersion        release version, such as 0.0.8 or 0.0.8-rc.1
;   AppNumericVersion the same as four numbers, such as 0.0.8.0
;   SourceDir         the packaged game folder (UOpenGolf.exe, UOpenGolf\, Engine\)
;   OutputDir, OutputBaseFilename

#ifndef AppVersion
  #error Build with build_installer.ps1, which passes AppVersion and the other defines.
#endif

#define AppName "UOpenGolf"
#define AppExe "UOpenGolf.exe"
#define GameExe "UOpenGolf\Binaries\Win64\UOpenGolf.exe"

[Setup]
; Never change AppId: it is how a newer Setup finds and upgrades an existing install.
AppId={{61A65027-869B-4F6E-B129-6BB1DE2BFB3E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=uopengolf contributors
AppPublisherURL=https://github.com/digitalhand/uopengolf
AppSupportURL=https://github.com/digitalhand/uopengolf/issues
AppUpdatesURL=https://github.com/digitalhand/uopengolf/releases
VersionInfoVersion={#AppNumericVersion}
; Per-user install: no administrator prompt, and the game can always update its own folder.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\{#AppName}
DisableProgramGroupPage=yes
; The folder is asked for on the first install only; upgrades reuse it.
DisableDirPage=auto
UsePreviousAppDir=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
; The game's .pak files are already compressed, so a slower setting saves little.
Compression=lzma2/fast
SolidCompression=no
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
SetupLogging=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Content is replaced as a whole, so a file dropped from a newer build can't be loaded from an older one.
Type: filesandordirs; Name: "{app}\UOpenGolf\Content\Paks"

[Files]
; Saved\ (settings, sign-in, logs) is never in a package, and Setup never removes files it didn't install, so it is kept.
; Debug symbols ship separately, as in the zip.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "\UOpenGolf\Saved,\Engine\Saved,*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; Offered as "Launch" after a normal install; always run after a silent in-game update, which restarts the game.
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall

[Code]
function IsLocked(const Path: String): Boolean;
var
  Stream: TFileStream;
begin
  Result := False;
  if not FileExists(Path) then Exit;
  try
    Stream := TFileStream.Create(Path, fmOpenReadWrite or fmShareExclusive);
    Stream.Free;
  except
    Result := True;
  end;
end;

{ An in-game update starts Setup and then quits; wait for the game to finish closing before files are replaced. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Waited: Integer;
begin
  Result := '';
  Waited := 0;
  while (IsLocked(ExpandConstant('{app}\{#AppExe}')) or IsLocked(ExpandConstant('{app}\{#GameExe}'))) and (Waited < 60000) do
  begin
    Sleep(500);
    Waited := Waited + 500;
  end;
  if Waited >= 60000 then
    Result := '{#AppName} is still running. Close it, then run this installer again.';
end;
