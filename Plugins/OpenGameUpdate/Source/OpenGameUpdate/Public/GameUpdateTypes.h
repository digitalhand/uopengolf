// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

/** How this copy of the game was installed, which decides what an update downloads and how it is applied. */
enum class EGameUpdateInstallKind : uint8
{
    /** Can't be updated from the game, such as in the editor. */
    None,
    /** Windows: the downloaded installer runs over the game's folder. */
    Installer,
    /** Linux portable folder: the release's archive replaces the folder. */
    Archive,
    /** Linux system package (deb, rpm): PackageInstaller installs it through pkexec. */
    Package,
};

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
    /** How this copy was installed. */
    EGameUpdateInstallKind InstallKind = EGameUpdateInstallKind::None;
    /** Linux: the installed package's format ("deb", "rpm"), from <InstallDir>/.package-format; empty otherwise. */
    FString PackageFormat;
    /** Linux: the package asset for PackageFormat, from PackageAsset_<format>; {version} as in InstallerAsset. */
    FString PackageAsset;
    /**
     * Linux: root-owned program pkexec runs as INSTALLER PACKAGE SUMS SIGNATURE. It installs only a package that
     * ChecksumListAsset lists and ChecksumSignatureAsset signs with the project's release key.
     */
    FString PackageInstaller;
    /** The release's checksum list (sha256sum format, such as SHA256SUMS). Package installs require it. */
    FString ChecksumListAsset;
    /** The release's detached OpenPGP signature of ChecksumListAsset, such as SHA256SUMS.asc. */
    FString ChecksumSignatureAsset;
    /** Linux: pkexec, which asks for the administrator password. */
    FString Pkexec;
    /** Linux: the plugin's apply-update.sh, which waits for the game to close, applies the update and restarts it. */
    FString UpdateHelper;

    bool IsValid() const;
    /** The download for a version: PackageAsset for a package install, otherwise InstallerAsset. */
    FString AssetName(const FString& Version) const;
    /** InstallerArguments with InstallDir filled in. */
    FString Arguments() const;
    /**
     * Reads [OpenGameUpdate] from the game config, including platform overrides such as Config/Linux/LinuxGame.ini.
     * The version is the project's ProjectVersion; InstallDir is the packaged game's folder, empty in the editor.
     */
    static FGameUpdateConfig Load();
};

/** One published release, as read from the update source. */
struct OPENGAMEUPDATE_API FGameUpdateRelease
{
    /** Without the leading "v", such as 0.0.x. */
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
    /** The signed checksum list and its signature (Config's ChecksumListAsset and ChecksumSignatureAsset). */
    FString SumsUrl;
    FString SumsSignatureUrl;

    bool HasInstaller() const { return !InstallerUrl.IsEmpty(); }
    bool IsSigned() const { return !SumsUrl.IsEmpty() && !SumsSignatureUrl.IsEmpty(); }
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
