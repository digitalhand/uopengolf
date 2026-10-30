// SPDX-License-Identifier: MIT
#include "Play/OpenGolfWindSetting.h"

namespace
{
    float WrapDeg(float Deg)
    {
        return FMath::IsFinite(Deg) ? FRotator::ClampAxis(Deg) : 0.0f;
    }
} // namespace

void OpenGolfPlay::FOpenGolfWindSetting::SetSpeedMph(float Mph)
{
    SpeedMph = FMath::Clamp(FMath::RoundToFloat(FMath::IsFinite(Mph) ? Mph / WindSpeedStepMph : 0.0f) * WindSpeedStepMph, 0.0f, MaxWindSettingMph);
}

void OpenGolfPlay::FOpenGolfWindSetting::SetFromDeg(float Deg)
{
    FromDeg = WrapDeg(FMath::GridSnap(FMath::IsFinite(Deg) ? Deg : 0.0f, WindDirectionStepDeg));
}

void OpenGolfPlay::FOpenGolfWindSetting::Adopt(float Mph, float InFromDeg)
{
    bOn = FMath::IsFinite(Mph) && Mph > 0.0f;
    if (bOn)
        SpeedMph = Mph;
    FromDeg = WrapDeg(InFromDeg);
}

FString OpenGolfPlay::WindDirectionLabel(float FromDeg)
{
    // Clockwise from a headwind: the wind from the front right blows into the player and from right to left.
    static const TCHAR* const Labels[] = {TEXT("Headwind"), TEXT("Into · right to left"), TEXT("Right to left"), TEXT("Helping · right to left"),
        TEXT("Tailwind"), TEXT("Helping · left to right"), TEXT("Left to right"), TEXT("Into · left to right")};
    return Labels[FMath::FloorToInt(WrapDeg(WrapDeg(FromDeg) + 22.5f) / 45.0f) % 8];
}
