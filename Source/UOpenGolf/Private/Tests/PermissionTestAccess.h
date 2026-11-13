// SPDX-License-Identifier: MIT
#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfOnlineSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
// Explicit synthetic account for gameplay automation; never a runtime bypass.
struct FOpenGolfPermissionTestAccess
{
    static void Set(UWorld* World, bool bAdmin = true)
    {
        auto* Online = World->GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>();
        Online->Permissions.Reset();
        if (bAdmin) Online->Permissions.Add(TEXT("ogolf:admin"));
        Online->AccessToken = TEXT("automation-only");
        Online->TokenExpiresAt = FPlatformTime::Seconds() + 3600;
    }
};
#endif
