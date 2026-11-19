// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * Unit factors between the SI core and launch-monitor and display units. The rounded values are the ones the
 * physics was calibrated with; keep them. Spin (rpm to rad/s) is GolfPhysics::RadPerRpm.
 */
namespace OpenFairway
{
    constexpr float MpsPerMph = 0.44704f;
    constexpr float YardsPerMeter = 1.09361f;
    constexpr float FeetPerMeter = 3.28084f;
    constexpr float MetersPerFoot = 0.3048f;
    /** Unreal world units (cm) per meter. */
    constexpr float CmPerM = 100.0f;
    constexpr float PaPerPsi = 6894.757f;
} // namespace OpenFairway
