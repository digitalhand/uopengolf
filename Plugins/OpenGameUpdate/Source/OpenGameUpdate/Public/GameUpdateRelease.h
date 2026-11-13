// SPDX-License-Identifier: MIT
#pragma once
#include "GameUpdateTypes.h"

namespace GameUpdate
{
    /**
     * Reads a GitHub "latest release" response. A release without Config's installer asset is still read, with
     * HasInstaller false. Sha256 is left empty: it is a separate file (ChecksumUrl). False, with Error, for anything
     * that is not a release.
     */
    OPENGAMEUPDATE_API bool ParseGitHubRelease(const FString& Json, const FGameUpdateConfig& Config, FGameUpdateRelease& Out, FString& Error);

    /**
     * The lowercase hex SHA-256 for FileName in sha256sum output ("<hash>  <name>" per line, or a lone hash).
     * Empty when it is not there.
     */
    OPENGAMEUPDATE_API FString ParseChecksum(const FString& Text, const FString& FileName);
}
