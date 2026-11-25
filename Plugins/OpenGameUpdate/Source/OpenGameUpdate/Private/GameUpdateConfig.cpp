// SPDX-License-Identifier: MIT
#include "GameUpdateTypes.h"
#include "GameUpdateInstall.h"
#include "GameUpdateLog.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
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
    const FString& Template = InstallKind == EGameUpdateInstallKind::Package ? PackageAsset : InstallerAsset;
    return Template.Replace(TEXT("{version}"), *Version);
}

namespace
{
    // Unreal's ini reader drops unquoted { and }, so an unquoted placeholder arrives as a plain word.
    void WarnWithout(const TCHAR* Key, const FString& Value, const TCHAR* Placeholder)
    {
        if (!Value.IsEmpty() && !Value.Contains(Placeholder))
            UE_LOG(LogGameUpdate, Warning, TEXT("%s \"%s\" has no %s. Put the value in quotes in the ini."), Key, *Value, Placeholder);
    }

#if PLATFORM_LINUX
    /** Linux: finds a package install from its marker and the plugin's helper script. */
    void LoadLinux(FGameUpdateConfig& C, const TCHAR* Section)
    {
        C.Pkexec = TEXT("/usr/bin/pkexec");
        GConfig->GetString(Section, TEXT("PackageInstaller"), C.PackageInstaller, GGameIni);
        if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenGameUpdate")))
            C.UpdateHelper = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources/Linux/apply-update.sh"));
        C.PackageFormat = GameUpdate::ReadPackageFormat(C.InstallDir);
        if (!C.PackageFormat.IsEmpty())
        {
            const FString Key = TEXT("PackageAsset_") + C.PackageFormat;
            GConfig->GetString(Section, *Key, C.PackageAsset, GGameIni);
            WarnWithout(*Key, C.PackageAsset, TEXT("{version}"));
            if (C.PackageAsset.IsEmpty())
                UE_LOG(LogGameUpdate, Warning, TEXT("This copy was installed as a %s package, but %s isn't configured."), *C.PackageFormat, *Key);
        }
        C.InstallKind = GameUpdate::LinuxInstallKind(C.InstallDir, C.PackageFormat, C.PackageAsset);
        C.DownloadDir = GameUpdate::LinuxDownloadDir(FApp::GetProjectName());
    }
#endif
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
    GConfig->GetString(Section, TEXT("ChecksumListAsset"), C.ChecksumListAsset, GGameIni);
    GConfig->GetString(Section, TEXT("ChecksumSignatureAsset"), C.ChecksumSignatureAsset, GGameIni);
    WarnWithout(TEXT("InstallerAsset"), C.InstallerAsset, TEXT("{version}"));
    WarnWithout(TEXT("InstallerArguments"), C.InstallerArguments, TEXT("{installdir}"));
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), C.CurrentVersion, GGameIni);
    C.CurrentVersion.TrimStartAndEndInline();
    const FString Project = FApp::GetProjectName();
    C.UserAgent = Project + TEXT("/") + (C.CurrentVersion.IsEmpty() ? TEXT("0") : C.CurrentVersion);
    // In the editor RootDir is the engine's folder, which must never be installed over.
    if (!GIsEditor) C.InstallDir = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
    C.DownloadDir = FPaths::Combine(FPlatformProcess::UserTempDir(), Project + TEXT("Update"));
#if PLATFORM_WINDOWS
    C.InstallKind = EGameUpdateInstallKind::Installer;
#elif PLATFORM_LINUX
    LoadLinux(C, Section);
#endif
    return C;
}
