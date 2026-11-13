// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

namespace OpenGolfOnline
{
    struct OPENGOLFONLINE_API FConfig
    {
        FString Authority;
        FString ClientId;
        FString Audience;
        FString ApiOrigin;
        FString MePath = TEXT("/api/v1/ogolf/me");
        FString ApplicationName;
        FString StorageName;
        TArray<int32> CallbackPorts;
        bool IsValid() const;
        static FConfig Load();
    };
}
