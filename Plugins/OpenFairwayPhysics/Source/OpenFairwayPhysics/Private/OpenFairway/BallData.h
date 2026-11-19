// SPDX-License-Identifier: MIT
// Measured drag of a modern golf ball against Reynolds number, near 2250 +/- 250 rpm: an approximate manual
// digitization of Lyu et al. 2020 (CC BY 4.0), Fig 3, rounded to about 0.01 in drag coefficient. Not raw
// experimental data. SpinRatioData.h uses its rise below Re 100k (the drag crisis); see Docs/Accuracy.md.
#pragma once

#include "CoreMath.h"

namespace OpenFairway::BallData
{
    constexpr float DragReynolds[] = {50000, 65000, 75000, 100000, 125000, 150000, 175000, 200000};
    constexpr float Drag[] = {.49f, .33f, .25f, .215f, .18f, .17f, .19f, .20f};
    static_assert(UE_ARRAY_COUNT(DragReynolds) == UE_ARRAY_COUNT(Drag), "One drag coefficient per Reynolds number");

    /** Measured drag at this Reynolds number, piecewise-linear and held at the ends. */
    inline float DragAt(float Reynolds)
    {
        return InterpolateHeld(DragReynolds, Drag, Reynolds);
    }
} // namespace OpenFairway::BallData
