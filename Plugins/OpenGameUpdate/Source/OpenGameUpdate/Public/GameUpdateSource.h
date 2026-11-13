// SPDX-License-Identifier: MIT
#pragma once
#include "GameUpdateTypes.h"
#include "Templates/SharedPointer.h"

/**
 * Where releases and their files come from: the network half of the updater. The subsystem only talks to this
 * interface, so tests pass a fake and other hosts (an own server, a store) can supply their own.
 * Callbacks run on the game thread. After Cancel, the cancelled request's callback is never called.
 */
class OPENGAMEUPDATE_API IGameUpdateSource
{
public:
    using FOnRelease = TFunction<void(bool bOk, const FGameUpdateRelease& Release, const FString& Error)>;
    using FOnProgress = TFunction<void(int64 Received)>;
    using FOnDone = TFunction<void(bool bOk, const FString& Error)>;

    virtual ~IGameUpdateSource() = default;
    /** The latest published release, with Sha256 filled in when it has an installer and a checksum file. */
    virtual void FetchLatest(FOnRelease OnDone) = 0;
    /** Streams Url to the file Path. */
    virtual void Download(const FString& Url, const FString& Path, FOnProgress OnProgress, FOnDone OnDone) = 0;
    virtual void Cancel() = 0;
};

namespace GameUpdate
{
    /** Releases from api.github.com for Config.Repository. Public repositories need no token. */
    OPENGAMEUPDATE_API TSharedRef<IGameUpdateSource> MakeGitHubSource(const FGameUpdateConfig& Config);
}
