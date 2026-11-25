// SPDX-License-Identifier: MIT
#pragma once
#include "GameUpdateTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameUpdateSubsystem.generated.h"

class IGameUpdateSource;

/**
 * Checks for a newer release, downloads and verifies its installer, and starts it.
 *
 * The host decides when to Check and draws its own UI from GetState and the getters, refreshing on OnChanged.
 * After DownloadAndInstall succeeds the installer is running and OnReadyToRestart fires: the host then quits, so the
 * installer can replace the game's files and start it again. The subsystem never quits the game itself.
 *
 * On Linux the "installer" is the plugin's apply-update.sh. It waits for the game to close, then either installs the
 * release's system package through pkexec (asking for the administrator password) or replaces a portable folder with
 * the release's archive, and starts the game again either way.
 */
UCLASS()
class OPENGAMEUPDATE_API UGameUpdateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Starts Program (the installer, or /bin/sh for Linux's helper) with Arguments; true if it started. */
    using FLaunchInstaller = TFunction<bool(const FString& Path, const FString& Arguments)>;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Replaces the config, source and launcher. Initialize uses the game config and GitHub; tests pass fakes. */
    void Configure(const FGameUpdateConfig& InConfig, TSharedRef<IGameUpdateSource> InSource, FLaunchInstaller InLauncher = {});

    /** Looks for a newer release. Ignored while a check or an update is running. */
    void Check();
    /** Whether the latest release can be installed from the game; otherwise offer OpenReleasePage. */
    bool CanInstall() const;
    /** How this copy updates; Package means the player is asked for the administrator password. */
    EGameUpdateInstallKind GetInstallKind() const { return Config.InstallKind; }
    /** Why the latest release can't be installed from the game, as one sentence for players; empty when it can. */
    FString GetInstallBlocker() const;
    /** Downloads, verifies and starts the installer. Requires CanInstall. */
    void DownloadAndInstall();
    /** Stops a check or a download. The update stays available. */
    void Cancel();
    /** The latest release's page in the browser, for its notes or a manual download. */
    void OpenReleasePage() const;

    EGameUpdateState GetState() const { return State; }
    /** A newer release was found, including while it is being downloaded or after a failed download. */
    bool IsUpdateAvailable() const { return bAvailable; }
    /** Checking, downloading, verifying or installing. */
    bool IsBusy() const;
    const FGameUpdateRelease& GetLatest() const { return Latest; }
    const FGameUpdateConfig& GetConfig() const { return Config; }
    /** Download progress from 0 to 1, or unset when the size is unknown. */
    TOptional<float> GetProgress() const;
    /** "412 MB of 587 MB  ·  24 MB/s" while downloading. */
    FString GetProgressText() const;
    /** Why the last check or update failed. */
    const FString& GetMessage() const { return Message; }
    /** Linux: why the update before this start didn't install, as the helper recorded it; empty otherwise. */
    const FString& GetLastInstallFailure() const { return LastInstallFailure; }

    FSimpleMulticastDelegate OnChanged;
    FSimpleMulticastDelegate OnReadyToRestart;

private:
    void SetState(EGameUpdateState InState, const FString& InMessage = {});
    void OnDownloaded(bool bOk, const FString& Error);
    void OnVerified(uint32 ForGeneration, const FString& Hash);
    void DeleteDownload();
    void ReadLastResult();
    /** Package installs: the release's checksum list and its signature, then the package. */
    void DownloadSignedList();
    void DownloadInstaller();
    bool Launch();

    FGameUpdateConfig Config;
    TSharedPtr<IGameUpdateSource> Source;
    FLaunchInstaller Launcher;
    EGameUpdateState State = EGameUpdateState::Idle;
    FGameUpdateRelease Latest;
    FString Message;
    FString DownloadPath;
    FString SumsPath;
    FString SignaturePath;
    FString LastInstallFailure;
    bool bAvailable = false;
    bool bInstallDirWritable = false;
    /** Linux archives replace the whole folder, so its parent must be writable too. */
    bool bInstallParentWritable = false;
    bool bHasHelper = false;
    bool bHasPkexec = false;
    bool bHasPackageInstaller = false;
    int64 Received = 0;
    double DownloadStarted = 0;
    /** Bumped by Cancel, so a verification that finishes afterwards is ignored. */
    uint32 Generation = 0;
};
