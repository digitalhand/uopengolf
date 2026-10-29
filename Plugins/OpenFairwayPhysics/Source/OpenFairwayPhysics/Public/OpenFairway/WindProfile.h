// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

namespace OpenFairway
{
    struct FFlightEnvironment;

    /**
     * Wind that grows with height over open ground (the neutral log wind profile). The authored wind
     * is the reading at the reference height, by default 10 m as in weather reports. Heights are
     * above the launch ground, not the ground beneath the ball.
     */
    namespace WindProfile
    {
        constexpr float StandardReferenceHeightM = 10.0f;
        /** Roughness length of open, short-grass ground. */
        constexpr float OpenGrassRoughnessM = 0.03f;
        /** Heights above this use the wind at this height. */
        constexpr float MaxHeightM = 100.0f;

        /**
         * Share of the reference wind at HeightM: ln(h / z0) / ln(h_ref / z0). Zero at or below the
         * roughness length and for non-finite heights; 1 (uniform wind) when the settings cannot
         * describe a profile.
         */
        OPENFAIRWAYPHYSICS_API float Factor(float HeightM, float ReferenceHeightM, float RoughnessM);

        /** ReferenceWind scaled by Factor. */
        OPENFAIRWAYPHYSICS_API FVector3f At(const FVector3f& ReferenceWind, float HeightM, float ReferenceHeightM, float RoughnessM);

        /** The environment's wind at HeightM, using its reference height and roughness. */
        OPENFAIRWAYPHYSICS_API FVector3f At(const FFlightEnvironment& Environment, float HeightM);
    } // namespace WindProfile
} // namespace OpenFairway
