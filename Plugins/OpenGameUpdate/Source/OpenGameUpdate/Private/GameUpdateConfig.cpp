// SPDX-License-Identifier: MIT
#include "GameUpdateTypes.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

bool FGameUpdateConfig::IsValid() const
{
    FString Owner, Name;
    return Repository.Split(TEXT("/"), &Owner, &Name) && !Owner.IsEmpty() && !Name.IsEmpty() && !Name.Contains(TEXT("/"))
        && !InstallerAsset.IsEmpty() && !UserAgent.IsEmpty();
}

FString FGameUpdateConfig::AssetName(const FString& Version) const
{
    return InstallerAsset.Replace(TEXT("{version}"), *Version);
}

FString FGameUpdateConfig::Arguments() const
{
    // A trailing backslash would escape the closing quote of "{installdir}".
    FString Dir = InstallDir;
    while (Dir.EndsWith(TEXT("/")) || Dir.EndsWith(TEXT("\\"))) Dir.LeftChopInline(1);
    FPaths::MakePlatformFilename(Dir);
    return InstallerArguments.Replace(TEXT("{installdir}"), *Dir);
}

FGameUpdateConfig FGameUpdateConfig::Load()
{
    FGameUpdateConfig C;
    const TCHAR* Section = TEXT("OpenGameUpdate");
    GConfig->GetString(Section, TEXT("Repository"), C.Repository, GGameIni);
    GConfig->GetString(Section, TEXT("InstallerAsset"), C.InstallerAsset, GGameIni);
    GConfig->GetString(Section, TEXT("InstallerArguments"), C.InstallerArguments, GGameIni);
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), C.CurrentVersion, GGameIni);
    C.CurrentVersion.TrimStartAndEndInline();
    const FString Project = FApp::GetProjectName();
    C.UserAgent = Project + TEXT("/") + (C.CurrentVersion.IsEmpty() ? TEXT("0") : C.CurrentVersion);
    // In the editor RootDir is the engine's folder, which must never be installed over.
    if (!GIsEditor) C.InstallDir = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
    C.DownloadDir = FPaths::Combine(FPlatformProcess::UserTempDir(), Project + TEXT("Update"));
    return C;
}
