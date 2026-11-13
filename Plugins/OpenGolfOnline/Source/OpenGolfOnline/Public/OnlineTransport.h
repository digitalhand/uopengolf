// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "OpenGolfOnlineConfig.h"
#include <atomic>

namespace OpenGolfOnline
{
    struct FHttpResult
    {
        int32 Status = 0;
        FString Body;
        bool bConnected = false;
    };

    struct FRequestCancellation
    {
        std::atomic<bool> Cancelled{false};
        std::atomic<bool> Finished{false};
        void Cancel() { Cancelled.store(true); }
    };
    using FCancelHandle = TSharedRef<FRequestCancellation, ESPMode::ThreadSafe>;
    using FHttpCallback = TFunction<void(FHttpResult)>;

    OPENGOLFONLINE_API bool IsAllowedRequest(const FConfig& Config, const FString& Url, const FString& Verb, const FString& AccessToken);
    /** Must be started on the game thread. Completion returns on that thread, including cancellation. */
    OPENGOLFONLINE_API FCancelHandle Request(const FConfig& Config, const FString& Url, const FString& Verb, const FString& Body,
        const FString& AccessToken, FHttpCallback Completion, bool bForm = false);
}
