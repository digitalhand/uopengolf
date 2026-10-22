// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

namespace OpenFairway
{
    /** Unclamped linear interpolation, as MathfCompat.Lerp in the C# core. */
    inline float Lerp(float From, float To, float Weight)
    {
        return From + (To - From) * Weight;
    }
} // namespace OpenFairway
