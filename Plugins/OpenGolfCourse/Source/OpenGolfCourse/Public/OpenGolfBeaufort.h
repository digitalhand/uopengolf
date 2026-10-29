// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * The Beaufort wind scale over land (WMO speeds, NOAA land descriptions), the reference for how wind
 * should look on the course. A force is set by the wind at the 10 m reference height, the reading the
 * range authors, not by the slower wind near the ground.
 *
 * The flag bands are this project's reading of each description for a light golf flag: the mean angle
 * of the flag's middle row below horizontal, from hoist to fly end. UOpenGolf.Course.FlagBeaufort holds
 * the cloth to them. vegetation_wind.py reads MinMps from this table for its motion onsets, and its
 * tests check that the two agree, so keep one row per line.
 */
namespace OpenGolfBeaufort
{
    struct FForce
    {
        int32 Number;
        /** Lowest 10 m wind of this force, m/s. */
        float MinMps;
        const TCHAR* Name;
        const TCHAR* Land;
        const TCHAR* Flag;
        float FlagDroopMinDeg;
        float FlagDroopMaxDeg;
    };

    // clang-format off
    inline constexpr FForce Scale[] = {
        {0, 0.0f, TEXT("Calm"), TEXT("Smoke rises straight up"), TEXT("Hangs limp"), 60.0f, 90.0f},
        {1, 0.3f, TEXT("Light air"), TEXT("Smoke drifts; leaves and vanes still"), TEXT("Hangs limp"), 60.0f, 90.0f},
        {2, 1.6f, TEXT("Light breeze"), TEXT("Leaves rustle; vanes begin to move"), TEXT("Stirs and lifts off the pole"), 30.0f, 75.0f},
        {3, 3.4f, TEXT("Gentle breeze"), TEXT("Leaves and small twigs in constant motion"), TEXT("Extended"), 0.0f, 35.0f},
        {4, 5.5f, TEXT("Moderate breeze"), TEXT("Dust raised; small branches move"), TEXT("Flaps along its length"), 0.0f, 20.0f},
        {5, 8.0f, TEXT("Fresh breeze"), TEXT("Small trees in leaf begin to sway"), TEXT("Flies out, flapping hard"), 0.0f, 15.0f},
        {6, 10.8f, TEXT("Strong breeze"), TEXT("Large branches in motion"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {7, 13.9f, TEXT("Near gale"), TEXT("Whole trees in motion"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {8, 17.2f, TEXT("Gale"), TEXT("Twigs break off trees"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {9, 20.8f, TEXT("Strong gale"), TEXT("Slight structural damage"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {10, 24.5f, TEXT("Storm"), TEXT("Trees uprooted"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {11, 28.5f, TEXT("Violent storm"), TEXT("Widespread damage"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
        {12, 32.7f, TEXT("Hurricane force"), TEXT("Devastation"), TEXT("Straight out, snapping"), 0.0f, 12.0f},
    };
    // clang-format on

    /** The force of a 10 m wind speed in m/s; non-finite or negative speeds read as calm. */
    OPENGOLFCOURSE_API const FForce& ForceFor(float WindMps);
} // namespace OpenGolfBeaufort
