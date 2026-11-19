// SPDX-License-Identifier: MIT
#include "OpenFairway/FlightAerodynamics.h"
#include "OpenFairway/GolfPhysics.h"

#include "CoreMath.h"
#include "SpinRatioData.h"

namespace OpenFairway
{
    namespace GroundDragCurve
    {
        // The drag polynomial and spin-drag multiplier of the ported flight model (ogolf-physics FlightAerodynamicsModel.cs,
        // with its high-Re step into the cap smoothed). Flight no longer uses them; roll-out and the stimp solve were
        // calibrated with them, so the ground keeps them.
        constexpr float CapReynolds = 200000.0f;
        constexpr float PolyA = 1.1948f;
        constexpr float PolyB = -0.0000209661f;
        constexpr float PolyC = 1.42472e-10f;
        constexpr float PolyD = -3.14383e-16f;
        constexpr float HighReCap = 0.2f;
        constexpr float HighReBlendStart = 175000.0f;
        constexpr float LowReFloor = 0.38f;
        constexpr float LowReBlendStart = 30000.0f;
        constexpr float At50k = 0.4632f;

        constexpr float SpinDragCoefficient = 4.0f;
        constexpr float SpinDragMax = 1.20f;
        constexpr float SpinDragHighSpinMax = 1.03f;
        constexpr float SpinDragUltraHighSpinMax = 1.21f;
        constexpr float HighSpinSrStart = 0.30f;
        constexpr float HighSpinSrEnd = 0.48f;
        constexpr float HighSpinReliefReFullMax = 90000.0f;
        constexpr float HighSpinReliefReZero = 105000.0f;
        constexpr float UltraHighSpinSrStart = 0.57f;
        constexpr float UltraHighSpinSrEnd = 0.77f;
        constexpr float ProgressiveCapSrStart = 0.33f;
        constexpr float ProgressiveCapSrEnd = 0.50f;
        constexpr float ProgressiveCapBoostMax = 0.25f;

        float Drag(float Reynolds)
        {
            if (Reynolds > CapReynolds)
            {
                return HighReCap;
            }
            if (Reynolds >= 50000.0f)
            {
                const float Polynomial = PolyA + PolyB * Reynolds + PolyC * Reynolds * Reynolds + PolyD * Reynolds * Reynolds * Reynolds;
                if (Reynolds <= HighReBlendStart)
                {
                    return Polynomial;
                }
                // Ease into the cap so drag has no step where the polynomial ends.
                return Lerp(Polynomial, HighReCap, SafeSmoothStep01(Reynolds, HighReBlendStart, CapReynolds));
            }
            if (Reynolds <= LowReBlendStart)
            {
                return LowReFloor;
            }
            return Lerp(LowReFloor, At50k, SafeSmoothStep01(Reynolds, LowReBlendStart, 50000.0f));
        }

        float SpinDragMultiplier(float SpinRatio, float Reynolds)
        {
            if (SpinRatio <= 0.0f)
            {
                return 1.0f;
            }
            const float HighSpinWeight = SafeSmoothStep01(SpinRatio, HighSpinSrStart, HighSpinSrEnd);
            const float ReReliefWeight = 1.0f - SafeSmoothStep01(Reynolds, HighSpinReliefReFullMax, HighSpinReliefReZero);
            float EffectiveCap = Lerp(SpinDragMax, SpinDragHighSpinMax, HighSpinWeight * ReReliefWeight);
            EffectiveCap += ProgressiveCapBoostMax * SafeSmoothStep01(SpinRatio, ProgressiveCapSrStart, ProgressiveCapSrEnd);
            EffectiveCap = Lerp(EffectiveCap, SpinDragUltraHighSpinMax, SafeSmoothStep01(SpinRatio, UltraHighSpinSrStart, UltraHighSpinSrEnd));
            return FMath::Min(1.0f + SpinDragCoefficient * SpinRatio * SpinRatio, EffectiveCap);
        }
    } // namespace GroundDragCurve

    bool FFlightAerodynamicsSample::HasAerodynamics() const
    {
        return Speed >= FlightAerodynamics::MinAerodynamicSpeed;
    }

    FFlightAerodynamicsSample FlightAerodynamics::Sample(const FVector3f& Velocity, const FVector3f& Omega, float AirDensity, float AirViscosity,
        const FFlightProfile& Profile)
    {
        FFlightAerodynamicsSample Result;
        const float Speed = Velocity.Size();
        Result.Speed = Speed;
        Result.SpinDecayRatePerSecond = FMath::Max(0.0f, Profile.SpinDecayCoefficient) * Speed / GolfPhysics::Radius;
        if (Speed < MinAerodynamicSpeed)
        {
            return Result;
        }

        Result.SpinRatio = Omega.Size() * GolfPhysics::Radius / Speed;
        Result.Reynolds = AirDensity * Speed * GolfPhysics::Radius * 2.0f / AirViscosity;
        Result.LiftCoefficient = SpinRatioData::LiftAt(Result.SpinRatio);
        Result.DragCoefficient = SpinRatioData::DragAt(Result.Reynolds, Result.SpinRatio);
        Result.bOutsideMeasuredCoverage = SpinRatioData::Outside(Result.Reynolds, Result.SpinRatio);
        return Result;
    }

    float FlightAerodynamics::GroundDragCoefficient(float Reynolds, float SpinRatio)
    {
        return GroundDragCurve::Drag(Reynolds) * GroundDragCurve::SpinDragMultiplier(SpinRatio, Reynolds);
    }
} // namespace OpenFairway
