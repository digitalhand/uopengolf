// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

/** Where updates come from and how the installer is run. Everything project-specific is here. */
struct OPENGAMEUPDATE_API FGameUpdateConfig
{
    /** GitHub "owner/name". Its latest published release (never a draft or prerelease) is the update. */
    FString Repository;
    /** Installer asset name in that release; {version} is the version without the leading "v". */
    FString InstallerAsset;
    /** Installer command line; {installdir} is InstallDir. */
    FString InstallerArguments;
    /** This build's version. */
    FString CurrentVersion;
    /** Folder the installer updates. Empty where an in-place install is impossible, such as the editor. */
    FString InstallDir;
    /** Where the installer is downloaded to. */
    FString DownloadDir;
    /** Sent with every request; GitHub requires one. */
    FString UserAgent;

    bool IsValid() const;
    /** InstallerAsset for a version. */
    FString AssetName(const FString& Version) const;
    /** InstallerArguments with InstallDir filled in. */
    FString Arguments() const;
    /**
     * Reads [OpenGameUpdate] Repository, InstallerAsset and InstallerArguments from the game config. The version is the
     * project's ProjectVersion; InstallDir is the packaged game's folder, empty in the editor.
     */
    static FGameUpdateConfig Load();
};

/** One published release, as read from the update source. */
struct OPENGAMEUPDATE_API FGameUpdateRelease
{
    /** Without the leading "v", such as 0.0.8. */
    FString Version;
    /** The release's web page, for notes or a manual download. */
    FString PageUrl;
    /** Release notes as written (Markdown). */
    FString Notes;
    FString InstallerName;
    FString InstallerUrl;
    int64 InstallerSize = 0;
    FString ChecksumUrl;
    /** Lowercase hex SHA-256 of the installer; empty when it could not be read. */
    FString Sha256;

    bool HasInstaller() const { return !InstallerUrl.IsEmpty(); }
};

enum class EGameUpdateState : uint8
{
    Idle,
    Checking,
    UpToDate,
    /** A newer release exists. */
    Available,
    Downloading,
    Verifying,
    /** The installer was started; the game should quit now. */
    Installing,
    /** See the message. After a failed download the update is still available and can be retried. */
    Failed,
};
