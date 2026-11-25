// SPDX-License-Identifier: MIT
#include "GameUpdateSubsystem.h"
#include "Async/Async.h"
#include "GameUpdateFormat.h"
#include "GameUpdateHash.h"
#include "GameUpdateInstall.h"
#include "GameUpdateLog.h"
#include "GameUpdateRelease.h"
#include "GameUpdateSource.h"
#include "GameUpdateVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
    /** Where Linux's helper records how the update went, for the next start. */
    const TCHAR* ResultFileName = TEXT("result");

    bool LaunchDetached(const FString& Program, const FString& Arguments)
    {
        FProcHandle Process = FPlatformProcess::CreateProc(*Program, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr);
        const bool bStarted = Process.IsValid();
        FPlatformProcess::CloseProc(Process);
        return bStarted;
    }

    bool CanWriteTo(const FString& Dir)
    {
        if (Dir.IsEmpty()) return false;
        const FString Probe = Dir / (TEXT(".update-probe-") + FGuid::NewGuid().ToString());
        if (!FFileHelper::SaveStringToFile(TEXT(""), *Probe))
        {
            UE_LOG(LogGameUpdate, Log, TEXT("Can't write to the install folder %s."), *Dir);
            return false;
        }
        IFileManager::Get().Delete(*Probe);
        return true;
    }
}

void UGameUpdateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FGameUpdateConfig Loaded = FGameUpdateConfig::Load();
    Configure(Loaded, GameUpdate::MakeGitHubSource(Loaded));
    // An installer from an earlier update is no longer needed. One still running keeps its own file: Windows locks
    // it, and Linux's helper records its process id while it works.
    if (!Config.InstallDir.IsEmpty() && !Config.DownloadDir.IsEmpty())
    {
        ReadLastResult();
        if (GameUpdate::IsHelperRunning(Config.DownloadDir))
        {
            UE_LOG(LogGameUpdate, Log, TEXT("An update is still being installed; its download is kept."));
        }
        else
        {
            IFileManager::Get().DeleteDirectory(*Config.DownloadDir, false, true);
        }
    }
}

void UGameUpdateSubsystem::ReadLastResult()
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(Config.DownloadDir / ResultFileName))) return;
    LastInstallFailure = GameUpdate::ParseHelperFailure(Text);
    if (LastInstallFailure.IsEmpty())
    {
        UE_LOG(LogGameUpdate, Log, TEXT("The last update finished: %s"), *Text.TrimStartAndEnd());
    }
    else
    {
        UE_LOG(LogGameUpdate, Warning, TEXT("The last update didn't install: %s"), *LastInstallFailure);
    }
}

void UGameUpdateSubsystem::Deinitialize()
{
    Cancel();
    Super::Deinitialize();
}

void UGameUpdateSubsystem::Configure(const FGameUpdateConfig& InConfig, TSharedRef<IGameUpdateSource> InSource, FLaunchInstaller InLauncher)
{
    if (Source) Source->Cancel();
    Config = InConfig;
    Source = InSource;
    Launcher = InLauncher ? MoveTemp(InLauncher) : FLaunchInstaller(&LaunchDetached);
    // Checked once here: CanInstall is read every frame by UI.
    // A system package's folder belongs to root; its installer runs elevated instead.
    bInstallDirWritable = Config.InstallKind != EGameUpdateInstallKind::Package && CanWriteTo(Config.InstallDir);
    bInstallParentWritable = Config.InstallKind == EGameUpdateInstallKind::Archive && bInstallDirWritable
        && CanWriteTo(FPaths::GetPath(GameUpdate::WithoutTrailingSlash(Config.InstallDir)));
    bHasHelper = FPaths::FileExists(Config.UpdateHelper);
    bHasPkexec = FPaths::FileExists(Config.Pkexec);
    bHasPackageInstaller = FPaths::FileExists(Config.PackageInstaller);
    State = EGameUpdateState::Idle;
    bAvailable = false;
    Latest = {};
    Message.Empty();
}

bool UGameUpdateSubsystem::IsBusy() const
{
    return State == EGameUpdateState::Checking || State == EGameUpdateState::Downloading
        || State == EGameUpdateState::Verifying || State == EGameUpdateState::Installing;
}

void UGameUpdateSubsystem::SetState(EGameUpdateState InState, const FString& InMessage)
{
    State = InState;
    Message = InMessage;
    if (!InMessage.IsEmpty()) UE_LOG(LogGameUpdate, Warning, TEXT("%s"), *InMessage);
    OnChanged.Broadcast();
}

void UGameUpdateSubsystem::Check()
{
    if (IsBusy() || !Source) return;
    if (!Config.IsValid()) { SetState(EGameUpdateState::Failed, TEXT("Updates are not configured for this game.")); return; }
    SetState(EGameUpdateState::Checking);
    Source->FetchLatest([Weak = TWeakObjectPtr<UGameUpdateSubsystem>(this)](bool bOk, const FGameUpdateRelease& Release, const FString& Error)
    {
        UGameUpdateSubsystem* This = Weak.Get();
        if (!This) return;
        if (!bOk) { This->bAvailable = false; This->SetState(EGameUpdateState::Failed, Error); return; }
        This->Latest = Release;
        This->bAvailable = GameUpdate::IsNewer(Release.Version, This->Config.CurrentVersion);
        UE_LOG(LogGameUpdate, Log, TEXT("Latest release %s, this build %s."), *Release.Version, *This->Config.CurrentVersion);
        if (This->bAvailable && !This->GetInstallBlocker().IsEmpty())
            UE_LOG(LogGameUpdate, Warning, TEXT("Can't install %s from the game: %s"), *Release.Version, *This->GetInstallBlocker());
        This->SetState(This->bAvailable ? EGameUpdateState::Available : EGameUpdateState::UpToDate);
    });
}

FString UGameUpdateSubsystem::GetInstallBlocker() const
{
    switch (Config.InstallKind)
    {
    case EGameUpdateInstallKind::None:
        return TEXT("Updating from the game isn't available for this copy.");
    case EGameUpdateInstallKind::Installer:
        if (!bInstallDirWritable) return TEXT("The game's folder can't be written to.");
        break;
    case EGameUpdateInstallKind::Archive:
        if (!bInstallDirWritable) return TEXT("The game's folder can't be written to.");
        if (!bInstallParentWritable) return TEXT("The folder that holds the game can't be written to.");
        if (!bHasHelper) return TEXT("The game's update helper is missing. Reinstall the game.");
        break;
    case EGameUpdateInstallKind::Package:
        if (!bHasPkexec) return TEXT("pkexec isn't installed, so the update can't ask for the administrator password.");
        if (!bHasPackageInstaller || !bHasHelper) return TEXT("The game's update helper is missing. Reinstall the game.");
        break;
    }
    if (!Latest.HasInstaller()) return Config.InstallKind == EGameUpdateInstallKind::Installer
        ? TEXT("This release has no installer.") : TEXT("This release has no download for this kind of install.");
    if (Latest.Sha256.IsEmpty()) return TEXT("The installer's checksum couldn't be read.");
    // The root installer only accepts a package the release key signed for.
    if (Config.InstallKind == EGameUpdateInstallKind::Package && !Latest.IsSigned())
        return TEXT("This release isn't signed, so it can't be installed as administrator.");
    return {};
}

bool UGameUpdateSubsystem::CanInstall() const
{
    return bAvailable && GetInstallBlocker().IsEmpty();
}

void UGameUpdateSubsystem::DownloadAndInstall()
{
    if (IsBusy() || !CanInstall()) return;
    LastInstallFailure.Empty();  // A new attempt replaces the earlier result.
    if (GameUpdate::IsHelperRunning(Config.DownloadDir))
    {
        SetState(EGameUpdateState::Failed, TEXT("An update is already being installed. Try again once the game restarts."));
        return;
    }
    const FString Dir = Config.DownloadDir / Latest.Version;
    IFileManager::Get().DeleteDirectory(*Config.DownloadDir, false, true);
    // On Linux a package is installed as root from here, so no other account may write to it.
    if (!GameUpdate::MakePrivateDirectory(Config.DownloadDir) || !IFileManager::Get().MakeDirectory(*Dir, true))
    {
        SetState(EGameUpdateState::Failed, TEXT("Could not create the download folder."));
        return;
    }
    DownloadPath = Dir / Latest.InstallerName;
    SumsPath = Dir / Config.ChecksumListAsset;
    SignaturePath = Dir / Config.ChecksumSignatureAsset;
    Received = 0;
    DownloadStarted = FPlatformTime::Seconds();
    SetState(EGameUpdateState::Downloading);
    if (Config.InstallKind == EGameUpdateInstallKind::Package)
        DownloadSignedList();
    else
        DownloadInstaller();
}

void UGameUpdateSubsystem::DownloadSignedList()
{
    // Both are a few hundred bytes, so a release that can't be installed fails before the package downloads.
    const TWeakObjectPtr<UGameUpdateSubsystem> Weak(this);
    const auto Then = [Weak](TFunction<void(UGameUpdateSubsystem&)> Next)
    {
        return [Weak, Next = MoveTemp(Next)](bool bOk, const FString& Error)
        {
            UGameUpdateSubsystem* This = Weak.Get();
            if (!This) return;
            if (!bOk) { This->SetState(EGameUpdateState::Failed, Error); return; }
            Next(*This);
        };
    };
    Source->Download(Latest.SumsUrl, SumsPath, [](int64) {}, Then([Then](UGameUpdateSubsystem& This)
    {
        This.Source->Download(This.Latest.SumsSignatureUrl, This.SignaturePath, [](int64) {}, Then([](UGameUpdateSubsystem& This)
        {
            // The root installer checks the signature; this catches a list that names another file early.
            FString Sums;
            if (!FFileHelper::LoadFileToString(Sums, *This.SumsPath)
                || GameUpdate::ParseChecksum(Sums, This.Latest.InstallerName) != This.Latest.Sha256)
            {
                This.SetState(EGameUpdateState::Failed, TEXT("The release's signed checksum list doesn't match its download."));
                return;
            }
            This.DownloadInstaller();
        }));
    }));
}

void UGameUpdateSubsystem::DownloadInstaller()
{
    const TWeakObjectPtr<UGameUpdateSubsystem> Weak(this);
    Source->Download(Latest.InstallerUrl, DownloadPath,
        [Weak](int64 Bytes) { if (UGameUpdateSubsystem* This = Weak.Get()) This->Received = Bytes; },
        [Weak](bool bOk, const FString& Error) { if (UGameUpdateSubsystem* This = Weak.Get()) This->OnDownloaded(bOk, Error); });
}

void UGameUpdateSubsystem::OnDownloaded(bool bOk, const FString& Error)
{
    if (!bOk) { DeleteDownload(); SetState(EGameUpdateState::Failed, Error); return; }
    SetState(EGameUpdateState::Verifying);
    // Hashing hundreds of megabytes takes seconds, so it runs off the game thread.
    const TWeakObjectPtr<UGameUpdateSubsystem> Weak(this);
    Async(EAsyncExecution::ThreadPool, [Weak, Path = DownloadPath, ForGeneration = Generation]
    {
        const FString Hash = GameUpdate::Sha256OfFile(Path);
        AsyncTask(ENamedThreads::GameThread, [Weak, ForGeneration, Hash]
        {
            if (UGameUpdateSubsystem* This = Weak.Get()) This->OnVerified(ForGeneration, Hash);
        });
    });
}

void UGameUpdateSubsystem::OnVerified(uint32 ForGeneration, const FString& Hash)
{
    if (ForGeneration != Generation || State != EGameUpdateState::Verifying) return;
    if (Hash != Latest.Sha256)
    {
        DeleteDownload();
        SetState(EGameUpdateState::Failed, TEXT("The download was damaged and has been deleted. Try again."));
        return;
    }
    if (!Launch())
    {
        SetState(EGameUpdateState::Failed, TEXT("Could not start the installer."));
        return;
    }
    UE_LOG(LogGameUpdate, Log, TEXT("Started the %s installer."), *Latest.Version);
    SetState(EGameUpdateState::Installing);
    OnReadyToRestart.Broadcast();
}

bool UGameUpdateSubsystem::Launch()
{
    if (Config.InstallKind == EGameUpdateInstallKind::Installer) return Launcher(DownloadPath, Config.Arguments());
    FGameUpdateHelperLaunch Helper;
    Helper.Helper = Config.UpdateHelper;
    Helper.Kind = Config.InstallKind;
    Helper.File = DownloadPath;
    Helper.InstallDir = Config.InstallDir;
    Helper.Version = Latest.Version;
    Helper.Pid = FPlatformProcess::GetCurrentProcessId();
    Helper.Pkexec = Config.Pkexec;
    Helper.PackageInstaller = Config.PackageInstaller;
    Helper.Sums = SumsPath;
    Helper.Signature = SignaturePath;
    Helper.ResultFile = Config.DownloadDir / ResultFileName;
    Helper.RestartProgram = FPlatformProcess::ExecutablePath();
    Helper.RestartArguments = FCommandLine::GetOriginal();
    const FString Arguments = GameUpdate::LinuxHelperArguments(Helper);
    if (Arguments.IsEmpty())
    {
        UE_LOG(LogGameUpdate, Warning, TEXT("A path for the update helper contains a double quote."));
        return false;
    }
    return Launcher(TEXT("/bin/sh"), Arguments);
}

void UGameUpdateSubsystem::Cancel()
{
    ++Generation;
    if (Source) Source->Cancel();
    const bool bWasDownloading = State == EGameUpdateState::Downloading || State == EGameUpdateState::Verifying;
    if (bWasDownloading) DeleteDownload();
    if (State == EGameUpdateState::Checking) SetState(EGameUpdateState::Idle);
    else if (bWasDownloading) SetState(EGameUpdateState::Available);
}

void UGameUpdateSubsystem::DeleteDownload()
{
    if (!DownloadPath.IsEmpty()) IFileManager::Get().Delete(*DownloadPath, false, true, true);
}

void UGameUpdateSubsystem::OpenReleasePage() const
{
    const FString Url = !Latest.PageUrl.IsEmpty() ? Latest.PageUrl
        : TEXT("https://github.com/") + Config.Repository + TEXT("/releases/latest");
    FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

TOptional<float> UGameUpdateSubsystem::GetProgress() const
{
    if (State == EGameUpdateState::Verifying || State == EGameUpdateState::Installing) return 1.f;
    if (State != EGameUpdateState::Downloading || Latest.InstallerSize <= 0) return {};
    return FMath::Clamp(float(double(Received) / double(Latest.InstallerSize)), 0.f, 1.f);
}

FString UGameUpdateSubsystem::GetProgressText() const
{
    if (State != EGameUpdateState::Downloading) return {};
    const double Elapsed = FPlatformTime::Seconds() - DownloadStarted;
    return GameUpdate::ProgressLine(Received, Latest.InstallerSize, Elapsed > 1 ? double(Received) / Elapsed : 0);
}
