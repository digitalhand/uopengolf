// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

namespace GameUpdate
{
    /** MAJOR.MINOR.PATCH with an optional -prerelease, as in Semantic Versioning. */
    struct OPENGAMEUPDATE_API FVersion
    {
        int32 Major = 0, Minor = 0, Patch = 0;
        FString Prerelease;

        /** Accepts a leading "v" and ignores +build metadata. False for anything else. */
        static bool Parse(const FString& Text, FVersion& Out);
        /** Negative, zero or positive, by Semantic Versioning precedence. */
        int32 Compare(const FVersion& Other) const;
    };

    /**
     * Whether Latest is a newer version than Current. A release is newer than its own prerelease or -dev build.
     * False when either cannot be read, so a broken version never prompts an update.
     */
    OPENGAMEUPDATE_API bool IsNewer(const FString& Latest, const FString& Current);
}
