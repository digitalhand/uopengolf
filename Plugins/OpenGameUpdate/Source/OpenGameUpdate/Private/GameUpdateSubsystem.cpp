// SPDX-License-Identifier: MIT
#include "GameUpdateSubsystem.h"
#include "Async/Async.h"
#include "GameUpdateFormat.h"
#include "GameUpdateHash.h"
#include "GameUpdateSource.h"
#include "GameUpdateVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameUpdate, Log, All);

namespace
{
    bool LaunchDetached(const FString& Path, const FString& Arguments)
    {
        FProcHandle Process = FPlatformProcess::CreateProc(*Path, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr);
        const bool bStarted = Process.IsValid();
        FPlatformProcess::CloseProc(Process);
        return bStarted;
    }

    bool CanWriteTo(const FString& Dir)
    {
        if (Dir.IsEmpty()) return false;
        const FString Probe = Dir / (TEXT(".update-probe-") + FGuid::NewGuid().ToString());
        if (!FFileHelper::SaveStringToFile(TEXT(""), *Probe)) return false;
        IFileManager::Get().Delete(*Probe);
        return true;
    }
}

void UGameUpdateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FGameUpdateConfig Loaded = FGameUpdateConfig::Load();
    Configure(Loaded, GameUpdate::MakeGitHubSource(Loaded));
    // An installer from an earlier update is no longer needed. One still running keeps its own file.
    if (!Config.InstallDir.IsEmpty() && !Config.DownloadDir.IsEmpty())
        IFileManager::Get().DeleteDirectory(*Config.DownloadDir, false, true);
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
    bInstallDirWritable = CanWriteTo(Config.InstallDir);
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
        This->SetState(This->bAvailable ? EGameUpdateState::Available : EGameUpdateState::UpToDate);
    });
}

bool UGameUpdateSubsystem::CanInstall() const
{
    return PLATFORM_WINDOWS && bAvailable && bInstallDirWritable && Latest.HasInstaller() && !Latest.Sha256.IsEmpty();
}

void UGameUpdateSubsystem::DownloadAndInstall()
{
    if (IsBusy() || !CanInstall()) return;
    const FString Dir = Config.DownloadDir / Latest.Version;
    IFileManager::Get().DeleteDirectory(*Config.DownloadDir, false, true);
    IFileManager::Get().MakeDirectory(*Dir, true);
    DownloadPath = Dir / Latest.InstallerName;
    Received = 0;
    DownloadStarted = FPlatformTime::Seconds();
    SetState(EGameUpdateState::Downloading);
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
    if (!Launcher(DownloadPath, Config.Arguments()))
    {
        SetState(EGameUpdateState::Failed, TEXT("Could not start the installer."));
        return;
    }
    UE_LOG(LogGameUpdate, Log, TEXT("Started the %s installer."), *Latest.Version);
    SetState(EGameUpdateState::Installing);
    OnReadyToRestart.Broadcast();
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
