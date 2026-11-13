// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "LaunchMonitorTypes.h"

namespace OpenGolfOnline
{
    enum class EFailure { None, SignedOut, Rejected, RetryLater, NotFound };
    EFailure ClassifyStatus(int32 Status);
    FString ClubKey(ELaunchMonitorClub Club);

    struct FRangeShot
    {
        FGuid Id;
        FGuid SessionId;
        FDateTime ShotAt;
        ELaunchMonitorClub Club = ELaunchMonitorClub::Driver;
        double CarryYards = 0;
        TOptional<double> RollYards;
        FLaunchMonitorShot Launch;
        TOptional<FLaunchMonitorClubData> ClubData;
    };

    /** Returns null for a shot the shared API cannot accept. */
    TSharedPtr<FJsonObject> EncodeShot(const FRangeShot& Shot);
}
