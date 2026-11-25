// SPDX-License-Identifier: MIT
#pragma once
#include "GameUpdateTypes.h"

/** What Linux's apply-update.sh is asked to do, as UGameUpdateSubsystem starts it. */
struct OPENGAMEUPDATE_API FGameUpdateHelperLaunch
{
    FString Helper;
    EGameUpdateInstallKind Kind = EGameUpdateInstallKind::Archive;
    /** The verified download. */
    FString File;
    FString InstallDir;
    /** The version being installed, written to the result file on success. */
    FString Version;
    /** The running game, which the helper waits for. */
    uint32 Pid = 0;
    /** Package installs: pkexec, the root-owned installer it runs, and the release's signed checksum list. */
    FString Pkexec;
    FString PackageInstaller;
    FString Sums;
    FString Signature;
    /** Where the helper writes "ok <version>" or "failed <reason>" for the next start. */
    FString ResultFile;
    /** The game to start again afterwards, and its command line exactly as Unreal holds it. */
    FString RestartProgram;
    FString RestartArguments;
};

namespace GameUpdate
{
    /** Name of the marker file a system package installs in the game's folder; it holds the format, such as "deb". */
    inline const TCHAR* PackageFormatFile = TEXT(".package-format");
    /** In the download folder: the running helper's process id, so a game started meanwhile keeps the download. */
    inline const TCHAR* HelperPidFile = TEXT("helper.pid");
    /** The longest failure reason shown to players; the rest of a package manager's output stays in its log. */
    constexpr int32 MaxFailureLength = 200;

    /** Path without trailing slashes; "/" stays "/". */
    OPENGAMEUPDATE_API FString WithoutTrailingSlash(FString Path);

    /**
     * The package format named by <InstallDir>/.package-format: one lowercase word such as "deb" or "rpm". Empty when
     * there is no marker or it holds anything else.
     */
    OPENGAMEUPDATE_API FString ReadPackageFormat(const FString& InstallDir);

    /**
     * How a Linux copy updates: Package when its format has a package asset, Archive for any other installed copy,
     * None without an install folder (the editor).
     */
    OPENGAMEUPDATE_API EGameUpdateInstallKind LinuxInstallKind(const FString& InstallDir, const FString& PackageFormat, const FString& PackageAsset);

    /**
     * Arguments for /bin/sh to run the helper, quoted the way Unreal's Unix CreateProc splits them. Empty when a path
     * contains a double quote, which can't be passed through.
     */
    OPENGAMEUPDATE_API FString LinuxHelperArguments(const FGameUpdateHelperLaunch& Launch);

    /**
     * The failure the helper recorded ("failed <reason>"), or empty after a success or without a result. A missing or
     * unreadable reason still reads as a failure. Reasons longer than MaxFailureLength are shortened.
     */
    OPENGAMEUPDATE_API FString ParseHelperFailure(const FString& ResultText);

    /** Linux: $XDG_CACHE_HOME (or ~/.cache) / <Project>Update. Private to the player, unlike the shared /var/tmp. */
    OPENGAMEUPDATE_API FString LinuxDownloadDir(const FString& Project);

    /** Creates Dir and, on Linux, limits it to its owner, so another account can't swap a download before it runs. */
    OPENGAMEUPDATE_API bool MakePrivateDirectory(const FString& Dir);

    /** Whether DownloadDir's helper.pid names a running process: an update from an earlier start is still installing. */
    OPENGAMEUPDATE_API bool IsHelperRunning(const FString& DownloadDir);
}
