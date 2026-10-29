// SPDX-License-Identifier: MIT
#include "OpenGolfBeaufort.h"

namespace OpenGolfBeaufort
{
    const FForce& ForceFor(float WindMps)
    {
        const float Speed = FMath::IsFinite(WindMps) ? WindMps : 0.0f;
        const FForce* Found = &Scale[0];
        for (const FForce& Force : Scale)
        {
            if (Speed >= Force.MinMps)
                Found = &Force;
        }
        return *Found;
    }
} // namespace OpenGolfBeaufort
