// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/FlightAerodynamicsModel.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#include "OpenFairway/GolfPhysics.h"

namespace OpenFairway
{
    namespace
    {
        constexpr int32 ReynoldsSamplePoints[] = {50000, 60000, 65000, 70000, 75000};

        float SmoothStep01(float T)
        {
            const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
            return Clamped * Clamped * (3.0f - 2.0f * Clamped);
        }

        /** Smooth step that returns a step instead of dividing by a degenerate (start == end) range. */
        float SafeSmoothStep01(float Value, float Start, float End)
        {
            const float Range = End - Start;
            if (FMath::Abs(Range) < 1e-6f)
            {
                return Value >= End ? 1.0f : 0.0f;
            }
            return SmoothStep01((Value - Start) / Range);
        }

        float Lerp(float A, float B, float T) { return A + (B - A) * T; }

        // Lift polynomial curves per Reynolds number; fixed fits, not profile values.
        float ClRe50k(float S) { return 0.0472121f + 2.84795f * S - 23.4342f * S * S + 45.4849f * S * S * S; }
        float ClRe60k(float S) { return 0.320524f - 4.7032f * S + 14.0613f * S * S; }
        float ClRe65k(float S) { return 0.266667f - 4.0f * S + 13.3333f * S * S; }
        float ClRe70k(float S) { return 0.0496189f + 0.00211396f * S + 2.34201f * S * S; }

        float GetHighReSpinGain(float SpinRatio, const FFlightProfile& P)
        {
            if (SpinRatio <= P.HighReGainReductionStart)
            {
                return P.HighReSpinGain;
            }
            if (SpinRatio < P.HighReGainReductionEnd)
            {
                return Lerp(P.HighReSpinGain, P.HighReMidSpinGain, SafeSmoothStep01(SpinRatio, P.HighReGainReductionStart, P.HighReGainReductionEnd));
            }
            if (SpinRatio <= P.HighReGainRecoveryStart)
            {
                return P.HighReMidSpinGain;
            }
            if (SpinRatio < P.HighReGainRecoveryEnd)
            {
                return Lerp(P.HighReMidSpinGain, P.HighReSpinGain, SafeSmoothStep01(SpinRatio, P.HighReGainRecoveryStart, P.HighReGainRecoveryEnd));
            }
            return P.HighReSpinGain;
        }

        float ClHighRe(float SpinRatio, const FFlightProfile& P)
        {
            const float Gain = GetHighReSpinGain(SpinRatio, P);
            return FlightAerodynamics::GetClMax(SpinRatio, P) * SpinRatio * Gain / (1.0f + SpinRatio * Gain);
        }

        float ClAtReynoldsIndex(int32 Index, float SpinRatio, const FFlightProfile& P)
        {
            switch (Index)
            {
            case 0:
                return ClRe50k(SpinRatio);
            case 1:
                return ClRe60k(SpinRatio);
            case 2:
                return ClRe65k(SpinRatio);
            case 3:
                return ClRe70k(SpinRatio);
            default:
                return ClHighRe(SpinRatio, P);
            }
        }

        float ApplyHighSpinLiftAttenuation(float SpinRatio, float Cl, const FFlightProfile& P)
        {
            const float Attenuation = 1.0f - P.HighSpinClAttenuationMax * SafeSmoothStep01(SpinRatio, P.HighSpinClAttenuationStart, P.HighSpinClAttenuationEnd);
            const float UltraAttenuation =
                1.0f - P.UltraHighSpinClAttenuationMax * SafeSmoothStep01(SpinRatio, P.UltraHighSpinClAttenuationStart, P.UltraHighSpinClAttenuationEnd);
            return Cl * Attenuation * UltraAttenuation;
        }

        float ApplyLowReHighSpinLiftAttenuation(float SpinRatio, float Cl, const FFlightProfile& P)
        {
            const float Attenuation = 1.0f - P.LowReHighSpinClAttenuationMax * SafeSmoothStep01(SpinRatio, P.HighSpinClAttenuationStart, P.HighSpinClAttenuationEnd);
            const float UltraAttenuation =
                1.0f - P.LowReUltraHighSpinClAttenuationMax * SafeSmoothStep01(SpinRatio, P.UltraHighSpinClAttenuationStart, P.UltraHighSpinClAttenuationEnd);
            return Cl * Attenuation * UltraAttenuation;
        }
    } // namespace

    bool FFlightAerodynamicsSample::HasAerodynamics() const
    {
        return Speed >= FlightAerodynamics::MinAerodynamicSpeed;
    }

    FFlightAerodynamicsSample FlightAerodynamics::Sample(const FVector3f& Velocity, const FVector3f& Omega, float AirDensity, float AirViscosity,
        float DragScale, float LiftScale, float InitialLaunchAngleDeg, const FFlightProfile& P)
    {
        FFlightAerodynamicsSample Result;
        const float Speed = Velocity.Size();
        if (Speed < MinAerodynamicSpeed)
        {
            return Result;
        }

        Result.Speed = Speed;
        Result.SpinRatio = Omega.Size() * GolfPhysics::Radius / Speed;
        Result.Reynolds = AirDensity * Speed * GolfPhysics::Radius * 2.0f / AirViscosity;
        Result.SpinDragMultiplier = GetSpinDragMultiplier(Result.SpinRatio, Result.Reynolds, P);
        Result.LowLaunchLiftScale = GetLowLaunchLiftScale(InitialLaunchAngleDeg, Result.SpinRatio, Result.Reynolds, P);
        const float HighLaunchDragScale = GetHighLaunchDragScale(InitialLaunchAngleDeg, Result.SpinRatio, P);
        Result.DragCoefficient = GetCd(Result.Reynolds, P) * Result.SpinDragMultiplier * DragScale * HighLaunchDragScale;
        Result.LiftCoefficient = GetCl(Result.Reynolds, Result.SpinRatio, P) * LiftScale * Result.LowLaunchLiftScale;
        Result.LiftCoefficient *= GetMidSpinClBoost(Result.SpinRatio, P);
        return Result;
    }

    float FlightAerodynamics::GetCd(float Reynolds, const FFlightProfile& P)
    {
        if (Reynolds > 200000.0f)
        {
            return P.HighReCdCap;
        }
        if (Reynolds >= 50000.0f)
        {
            return P.CdPolyA + P.CdPolyB * Reynolds + P.CdPolyC * Reynolds * Reynolds + P.CdPolyD * Reynolds * Reynolds * Reynolds;
        }
        if (Reynolds <= P.LowReBlendStart)
        {
            return P.LowReCdFloor;
        }
        return Lerp(P.LowReCdFloor, P.CdAt50k, SafeSmoothStep01(Reynolds, P.LowReBlendStart, 50000.0f));
    }

    float FlightAerodynamics::GetCl(float Reynolds, float SpinRatio, const FFlightProfile& P)
    {
        const float Spin = FMath::Max(0.0f, SpinRatio);
        if (Spin <= 0.0f)
        {
            return 0.0f;
        }

        const float DynamicClMax = GetClMax(Spin, P);
        if (Reynolds < 50000.0f)
        {
            if (Reynolds <= 30000.0f)
            {
                return 0.0f;
            }
            const float LowReT = SmoothStep01((Reynolds - 30000.0f) / 20000.0f);
            const float ClAt50k = FMath::Clamp(ClRe50k(Spin), 0.0f, DynamicClMax);
            return ApplyLowReHighSpinLiftAttenuation(Spin, ClAt50k * LowReT, P);
        }

        if (Reynolds >= P.HighReStart)
        {
            return ApplyHighSpinLiftAttenuation(Spin, FMath::Clamp(ClHighRe(Spin, P), 0.0f, DynamicClMax), P);
        }

        constexpr int32 Count = UE_ARRAY_COUNT(ReynoldsSamplePoints);
        int32 HighIndex = Count - 1;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (Reynolds <= ReynoldsSamplePoints[Index])
            {
                HighIndex = Index;
                break;
            }
        }
        const int32 LowIndex = FMath::Max(HighIndex - 1, 0);

        const float ClLow = FMath::Max(0.0f, ClAtReynoldsIndex(LowIndex, Spin, P));
        const float ClHigh = FMath::Max(0.0f, ClAtReynoldsIndex(HighIndex, Spin, P));
        const float ReLow = static_cast<float>(ReynoldsSamplePoints[LowIndex]);
        const float ReHigh = static_cast<float>(ReynoldsSamplePoints[HighIndex]);
        const float Weight = ReHigh != ReLow ? (Reynolds - ReLow) / (ReHigh - ReLow) : 0.0f;
        const float Clamped = FMath::Clamp(Lerp(ClLow, ClHigh, Weight), 0.0f, DynamicClMax);
        return ApplyLowReHighSpinLiftAttenuation(Spin, Clamped, P);
    }

    float FlightAerodynamics::GetSpinDragMultiplier(float SpinRatio, float Reynolds, const FFlightProfile& P)
    {
        if (SpinRatio <= 0.0f)
        {
            return 1.0f;
        }

        const float HighSpinWeight = SafeSmoothStep01(SpinRatio, P.HighSpinDragSrStart, P.HighSpinDragSrEnd);
        const float ReReliefWeight = 1.0f - SafeSmoothStep01(Reynolds, P.HighSpinDragReliefReFullMax, P.HighSpinDragReliefReZero);
        float EffectiveCap = Lerp(P.SpinDragMultiplierMax, P.SpinDragMultiplierHighSpinMax, HighSpinWeight * ReReliefWeight);
        EffectiveCap += P.SpinDragProgressiveCapBoostMax * SafeSmoothStep01(SpinRatio, P.SpinDragProgressiveCapSrStart, P.SpinDragProgressiveCapSrEnd);
        EffectiveCap = Lerp(EffectiveCap, P.SpinDragMultiplierUltraHighSpinMax, SafeSmoothStep01(SpinRatio, P.UltraHighSpinDragSrStart, P.UltraHighSpinDragSrEnd));
        return FMath::Min(1.0f + P.SpinDragMultiplierCoeff * SpinRatio * SpinRatio, EffectiveCap);
    }

    float FlightAerodynamics::GetLowLaunchLiftScale(float InitialLaunchAngleDeg, float SpinRatio, float Reynolds, const FFlightProfile& P)
    {
        const float LaunchFactor = SafeSmoothStep01(P.LowLaunchVlaZeroDeg - InitialLaunchAngleDeg, 0.0f, P.LowLaunchVlaZeroDeg - P.LowLaunchVlaFullDeg);
        if (LaunchFactor <= 0.0f)
        {
            return 1.0f;
        }
        const float ReFactor = SafeSmoothStep01(Reynolds, P.LowLaunchReStart, P.LowLaunchReEnd);
        if (ReFactor <= 0.0f)
        {
            return 1.0f;
        }
        const float SpinFactor = 1.0f - SafeSmoothStep01(SpinRatio, P.LowLaunchSpinRatioFull, P.LowLaunchSpinRatioMax);
        if (SpinFactor <= 0.0f)
        {
            return 1.0f;
        }
        return Lerp(1.0f, P.LowLaunchLiftRecoveryMax, LaunchFactor * ReFactor * SpinFactor);
    }

    float FlightAerodynamics::GetHighLaunchDragScale(float InitialLaunchAngleDeg, float SpinRatio, const FFlightProfile& P)
    {
        const float LaunchFactor = SafeSmoothStep01(InitialLaunchAngleDeg, P.HighLaunchDragVlaStartDeg, P.HighLaunchDragVlaFullDeg);
        if (LaunchFactor <= 0.0f)
        {
            return 1.0f;
        }
        const float SpinFactor = SafeSmoothStep01(SpinRatio, P.HighLaunchDragSrStart, P.HighLaunchDragSrEnd);
        if (SpinFactor <= 0.0f)
        {
            return 1.0f;
        }
        return Lerp(1.0f, P.HighLaunchDragBoostMax, LaunchFactor * SpinFactor);
    }

    float FlightAerodynamics::GetMidSpinClBoost(float SpinRatio, const FFlightProfile& P)
    {
        if (P.MidSpinClBoostMax <= 0.0f)
        {
            return 1.0f;
        }
        const float T = SafeSmoothStep01(SpinRatio, P.MidSpinClBoostSrStart, P.MidSpinClBoostSrEnd);
        return 1.0f + P.MidSpinClBoostMax * T * (1.0f - T) * 4.0f;
    }

    float FlightAerodynamics::GetClMax(float SpinRatio, const FFlightProfile& P)
    {
        if (SpinRatio <= P.ClMaxSrTransitionStart)
        {
            return P.ClMaxBase;
        }
        if (SpinRatio >= P.ClMaxSrTransitionEnd)
        {
            return P.ClMaxHighSpin;
        }
        return Lerp(P.ClMaxBase, P.ClMaxHighSpin, SafeSmoothStep01(SpinRatio, P.ClMaxSrTransitionStart, P.ClMaxSrTransitionEnd));
    }
} // namespace OpenFairway
