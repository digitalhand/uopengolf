// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

class AOpenFairwayBall;
class AUOpenGolfRange;

namespace WindDisplay
{
    struct FState
    {
        float SpeedMph = 0;
        FVector2D Direction = FVector2D(0, 1);

        FString SpeedLabel() const;
    };

    FState FromVelocity(const FVector& WindMps, float ViewYawDeg);
    FState FromSettings(float SpeedMph, float FromDeg, float ViewYawDeg);
    FState Read(const AOpenFairwayBall* Ball, const AUOpenGolfRange* Range, float ViewYawDeg);
} // namespace WindDisplay
