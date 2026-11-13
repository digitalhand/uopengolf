// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

namespace OpenGolfOnline
{
    enum class EStoreResult { Ok, Empty, Unavailable };

    /** Call on a worker thread: the operating system may prompt to unlock its keyring. */
    OPENGOLFONLINE_API EStoreResult ReadRefreshToken(const FString& StorageName, FString& Token);
    OPENGOLFONLINE_API EStoreResult WriteRefreshToken(const FString& StorageName, const FString& Token);
    OPENGOLFONLINE_API EStoreResult ClearRefreshToken(const FString& StorageName);
}
