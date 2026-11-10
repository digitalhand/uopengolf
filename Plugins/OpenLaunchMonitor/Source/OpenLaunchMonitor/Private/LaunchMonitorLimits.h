// SPDX-License-Identifier: MIT
// Plausibility limits shared by every provider's decoder, so all of them drop the same implausible shots.
#pragma once

#include "CoreMinimal.h"

namespace LaunchMonitorLimits
{
    inline constexpr float MaxBallSpeedMps = 250.0f;
    inline constexpr int32 MaxSpinRpm = 30000;
    /** A putt or topped shot can launch slightly downward; a quarter turn or more is garbage. */
    inline constexpr float MaxAbsVerticalAngleDeg = 90.0f;
    inline constexpr float MaxAbsHorizontalAngleDeg = 90.0f;
    inline constexpr float MaxAbsSpinAxisDeg = 180.0f;
    inline constexpr float MaxAbsClubAngleDeg = 45.0f;
    inline constexpr float MinDynamicLoftDeg = -10.0f;
    inline constexpr float MaxDynamicLoftDeg = 90.0f;
    inline constexpr float MaxImpactHorizontalMm = 40.0f;
    inline constexpr float MaxImpactVerticalMm = 30.0f;
} // namespace LaunchMonitorLimits
