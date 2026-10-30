// SPDX-License-Identifier: MIT
#include "Play/OpenGolfPlayRules.h"

namespace
{
    constexpr float FeetPerMeter = 3.28084f;
    constexpr float YardsPerMeter = 1.0936133f;
    // A held aim key turns only after a tap's length, then ramps from the slow to the fast rate.
    constexpr float AimHoldDelayS = 0.3f;
    constexpr float AimRampS = 1.0f;
    constexpr float SlowAimDegPerSecond = 2.0f;
    constexpr float FastAimDegPerSecond = 20.0f;
    // Offsets under this read as straight; whole degrees from WholeAimDeg up.
    constexpr float StraightAimDeg = 0.05f;
    constexpr float WholeAimDeg = 10.0f;
} // namespace

float OpenGolfPlay::BearingDeg(const FVector& From, const FVector& To, float Fallback)
{
    const FVector Delta = To - From;
    return Delta.SizeSquared2D() > 1.0 ? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X))) : Fallback;
}

bool OpenGolfPlay::IsShortGame(float DistanceToCupM, bool bOnGreen)
{
    return bOnGreen || DistanceToCupM < ShortGameDistanceM;
}

float OpenGolfPlay::CupZoomAmount(float DistanceM)
{
    return FMath::SmoothStep(0.0f, 1.0f, 1.0f - DistanceM / CupZoomStartM);
}

bool OpenGolfPlay::CanPlaceBallAt(const FVector& Location, const FVector& Cup)
{
    return FVector::Dist2D(Location, Cup) >= MinPlaceDistanceFromCupM * 100.0f;
}

FString OpenGolfPlay::DistanceToPinLabel(float DistanceM, bool bOnGreen)
{
    if (bOnGreen)
        return FString::Printf(TEXT("%d ft"), FMath::RoundToInt(DistanceM * FeetPerMeter));
    return FString::Printf(TEXT("%d yd"), FMath::RoundToInt(DistanceM * YardsPerMeter));
}

float OpenGolfPlay::AimTurnDegPerSecond(float HeldSeconds, bool bFine)
{
    if (HeldSeconds < AimHoldDelayS)
        return 0.0f;
    const float Ramp = FMath::SmoothStep(0.0f, 1.0f, (HeldSeconds - AimHoldDelayS) / AimRampS);
    return bFine ? SlowAimDegPerSecond : FMath::Lerp(SlowAimDegPerSecond, FastAimDegPerSecond, Ramp);
}

FString OpenGolfPlay::AimOffsetLabel(float OffsetDeg, float TargetDistanceM, bool bShortGame)
{
    const float Degrees = FMath::Abs(OffsetDeg);
    if (Degrees < StraightAimDeg)
        return TEXT("Straight");
    const TCHAR* Side = OffsetDeg < 0.0f ? TEXT("L") : TEXT("R");
    FString Label = Degrees < WholeAimDeg ? FString::Printf(TEXT("%.1f° %s"), Degrees, Side) : FString::Printf(TEXT("%d° %s"), FMath::RoundToInt(Degrees), Side);
    if (TargetDistanceM > 0.0f)
    {
        const float LateralM = TargetDistanceM * FMath::Tan(FMath::DegreesToRadians(Degrees));
        Label += bShortGame ? FString::Printf(TEXT(" · %.1f ft"), LateralM * FeetPerMeter) : FString::Printf(TEXT(" · %d yd"), FMath::RoundToInt(LateralM * YardsPerMeter));
    }
    return Label;
}
