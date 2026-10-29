// SPDX-License-Identifier: MIT
#include "OpenFairway/WindProfile.h"
#include "OpenFairway/Profiles.h"

namespace OpenFairway
{
    float WindProfile::Factor(float HeightM, float ReferenceHeightM, float RoughnessM)
    {
        const bool bProfile = FMath::IsFinite(ReferenceHeightM) && FMath::IsFinite(RoughnessM) && RoughnessM > 0.0f && ReferenceHeightM > RoughnessM;
        if (!bProfile)
        {
            return 1.0f;
        }
        if (!FMath::IsFinite(HeightM) || HeightM <= RoughnessM)
        {
            return 0.0f;
        }
        // Differences of logarithms, so a tiny roughness cannot overflow Height / Roughness.
        const float LogRoughness = FMath::Loge(RoughnessM);
        const float Height = FMath::Min(HeightM, MaxHeightM);
        return (FMath::Loge(Height) - LogRoughness) / (FMath::Loge(ReferenceHeightM) - LogRoughness);
    }

    FVector3f WindProfile::At(const FVector3f& ReferenceWind, float HeightM, float ReferenceHeightM, float RoughnessM)
    {
        return ReferenceWind * Factor(HeightM, ReferenceHeightM, RoughnessM);
    }

    FVector3f WindProfile::At(const FFlightEnvironment& Environment, float HeightM)
    {
        return At(Environment.Wind, HeightM, Environment.WindReferenceHeightM, Environment.WindRoughnessM);
    }
} // namespace OpenFairway
