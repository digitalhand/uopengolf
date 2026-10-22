// SPDX-License-Identifier: MIT
#include "WindDisplay.h"

#include "OpenFairwayBall.h"
#include "UOpenGolfRange.h"

namespace WindDisplay
{
    FString FState::SpeedLabel() const
    {
        return FString::Printf(TEXT("%.0f MPH"), FMath::RoundToDouble(SpeedMph));
    }

    FState FromVelocity(const FVector& WindMps, float ViewYawDeg)
    {
        FState State;
        if (WindMps.ContainsNaN())
            return State;
        const FVector Wind(WindMps.X, WindMps.Y, 0);
        State.SpeedMph = Wind.Size() / 0.44704;
        if (Wind.IsNearlyZero())
            return State;
        const FRotator View(0, FMath::IsFinite(ViewYawDeg) ? ViewYawDeg : 0, 0);
        const FVector Forward = View.RotateVector(FVector::ForwardVector);
        const FVector Right = View.RotateVector(FVector::RightVector);
        State.Direction = FVector2D(FVector::DotProduct(Wind, Right), -FVector::DotProduct(Wind, Forward)).GetSafeNormal();
        return State;
    }

    FState FromSettings(float SpeedMph, float FromDeg, float ViewYawDeg)
    {
        const float Speed = FMath::IsFinite(SpeedMph) ? FMath::Max(0.0f, SpeedMph) : 0.0f;
        const float Angle = FMath::DegreesToRadians(FMath::IsFinite(FromDeg) ? FRotator::ClampAxis(FromDeg) : 0.0f);
        return FromVelocity(FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0) * Speed * 0.44704, ViewYawDeg);
    }

    FState Read(const AOpenFairwayBall* Ball, const AUOpenGolfRange* Range, float ViewYawDeg)
    {
        if (Ball)
            return FromVelocity(Ball->GetWindVelocityMps(), ViewYawDeg);
        return Range ? FromSettings(Range->WindSpeedMph, Range->WindFromDeg, ViewYawDeg) : FState();
    }
} // namespace WindDisplay
