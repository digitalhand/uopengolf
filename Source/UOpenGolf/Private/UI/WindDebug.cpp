// SPDX-License-Identifier: MIT
#include "UI/WindDebug.h"

#include "OpenFairway/ShotSimulator.h"
#include "OpenFairway/WindProfile.h"
#include "OpenGolfBeaufort.h"
#include "OpenGolfFlagstickComponent.h"

namespace WindDebug
{
    namespace
    {
        float MphAt(const FInput& Input, float SpeedMps, float HeightM)
        {
            return SpeedMps * OpenFairway::WindProfile::Factor(HeightM, Input.ReferenceHeightM, Input.RoughnessM) / OpenFairway::MpsPerMph;
        }
    } // namespace

    TArray<FString> Lines(const FInput& Input)
    {
        const float SpeedMps = Input.WindMps.ContainsNaN() ? 0.0f : static_cast<float>(Input.WindMps.Size2D());
        // The Beaufort scale reads the wind at 10 m, whatever height the wind was authored at.
        const float StandardMps = SpeedMps * OpenFairway::WindProfile::Factor(OpenFairway::WindProfile::StandardReferenceHeightM,
            Input.ReferenceHeightM, Input.RoughnessM);
        const OpenGolfBeaufort::FForce& Force = OpenGolfBeaufort::ForceFor(StandardMps);
        constexpr float FlagHeightM = UOpenGolfFlagstickComponent::FlagCentreHeightCm / 100.0f;
        TArray<FString> Out;
        Out.Add(FString::Printf(TEXT("Wind at 10 m: %.1f mph (%.1f m/s), Beaufort %d %s"), StandardMps / OpenFairway::MpsPerMph, StandardMps,
            Force.Number, Force.Name));
        Out.Add(FString::Printf(TEXT("  Land: %s"), Force.Land));
        Out.Add(FString::Printf(TEXT("  At height: grass %.1f m %.1f mph, flag %.1f m %.1f mph, crown %.0f m %.1f mph"), RangeGrassHeightM,
            MphAt(Input, SpeedMps, RangeGrassHeightM), FlagHeightM, MphAt(Input, SpeedMps, FlagHeightM), RangeCrownHeightM,
            MphAt(Input, SpeedMps, RangeCrownHeightM)));
        if (Input.bHasFlag)
        {
            const bool bInBand = Input.FlagDroopDeg >= Force.FlagDroopMinDeg && Input.FlagDroopDeg <= Force.FlagDroopMaxDeg;
            Out.Add(FString::Printf(TEXT("  Flag: %.0f deg droop, %.1f flaps/s; expected %s (%.0f-%.0f deg): %s"), Input.FlagDroopDeg,
                Input.FlagFlapHz, Force.Flag, Force.FlagDroopMinDeg, Force.FlagDroopMaxDeg, bInBand ? TEXT("in band") : TEXT("OUT OF BAND")));
        }
        return Out;
    }
} // namespace WindDebug
