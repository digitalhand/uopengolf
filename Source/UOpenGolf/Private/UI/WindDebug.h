// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "OpenFairway/WindProfile.h"

/**
 * The uopengolf.WindDebug overlay: the authored wind against the Beaufort scale, the wind at the heights
 * where grass, the flag and tree crowns meet it, and the flag's measured droop and flap rate beside what
 * the scale expects of a light flag at that force.
 */
namespace WindDebug
{
    /** The range's heights for the overlay's wind-at-height line: tall-rough tips and mid-crown of its jacarandas. */
    constexpr float RangeGrassHeightM = 0.3f;
    constexpr float RangeCrownHeightM = 6.0f;

    struct FInput
    {
        /** World wind at the reference height, m/s, and the height profile it is read through. */
        FVector WindMps = FVector::ZeroVector;
        float ReferenceHeightM = OpenFairway::WindProfile::StandardReferenceHeightM;
        float RoughnessM = OpenFairway::WindProfile::OpenGrassRoughnessM;
        bool bHasFlag = false;
        float FlagDroopDeg = 90.0f;
        float FlagFlapHz = 0.0f;
    };

    TArray<FString> Lines(const FInput& Input);
} // namespace WindDebug
