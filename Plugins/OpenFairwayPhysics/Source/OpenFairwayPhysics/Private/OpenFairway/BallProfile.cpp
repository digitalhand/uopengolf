// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/BallPhysicsProfile.cs, ShotRegimeKey.cs,
// LandingBucket.cs and PhysicsParamsFactory.cs at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#include "OpenFairway/BallProfile.h"

#include "OpenFairway/GolfPhysics.h"

namespace OpenFairway
{
    namespace
    {
        constexpr float FeetToMeters = 0.3048f;

        FRegimeScaleOverride Scale(float Drag, float Lift = 1.0f)
        {
            FRegimeScaleOverride Override;
            Override.DragScaleMultiplier = Drag;
            Override.LiftScaleMultiplier = Lift;
            return Override;
        }

        const FRegimeScaleOverride& NeutralRegime()
        {
            static const FRegimeScaleOverride Neutral;
            return Neutral;
        }

        // Interior samples of the existing calibration regions, including the extra family
        // splits at 110 mph and 30 degrees. Linear interpolation is continuous at every
        // knot and avoids concentrating a large correction into a narrow boundary band.
        constexpr float SpeedAnchors[] = {54.0f, 66.0f, 78.5f, 95.0f, 107.5f, 115.0f, 125.0f};
        constexpr float LaunchAnchors[] = {5.0f, 14.0f, 21.5f, 27.5f, 31.5f, 36.5f};
        constexpr float SpinAnchors[] = {1250.0f, 3250.0f, 4750.0f, 6500.0f, 8500.0f};

        struct FBlendAxis
        {
            float Values[2];
            float Weights[2];
        };

        template <int32 Count>
        FBlendAxis Bracket(const float (&Anchors)[Count], float Value)
        {
            if (Value <= Anchors[0])
            {
                return {{Anchors[0], Anchors[0]}, {1.0f, 0.0f}};
            }
            for (int32 Index = 1; Index < Count; ++Index)
            {
                if (Value < Anchors[Index])
                {
                    const float T = (Value - Anchors[Index - 1]) / (Anchors[Index] - Anchors[Index - 1]);
                    return {{Anchors[Index - 1], Anchors[Index]}, {1.0f - T, T}};
                }
            }
            return {{Anchors[Count - 1], Anchors[Count - 1]}, {1.0f, 0.0f}};
        }

        const FRegimeScaleOverride& LookupRegime(const FBallPhysicsProfile& Profile, float SpeedMph,
            float LaunchAngleDeg, float TotalSpinRpm, FString& Matched)
        {
            Matched.Reset();
            for (const FString& Candidate : ShotRegimeKey::BuildLookupKeys(SpeedMph, LaunchAngleDeg, TotalSpinRpm))
            {
                if (const FRegimeScaleOverride* Found = Profile.RegimeScaleOverrides.Find(Candidate))
                {
                    Matched = Candidate;
                    return *Found;
                }
            }
            return NeutralRegime();
        }

        void Accumulate(FRegimeScaleOverride& Sum, const FRegimeScaleOverride& Value, float Weight)
        {
            Sum.DragScaleMultiplier += Value.DragScaleMultiplier * Weight;
            Sum.LiftScaleMultiplier += Value.LiftScaleMultiplier * Weight;
            Sum.KineticFrictionMultiplier += Value.KineticFrictionMultiplier * Weight;
            Sum.RollingFrictionMultiplier += Value.RollingFrictionMultiplier * Weight;
            Sum.GrassViscosityMultiplier += Value.GrassViscosityMultiplier * Weight;
            Sum.CriticalAngleOffsetRadians += Value.CriticalAngleOffsetRadians * Weight;
            Sum.SpinbackThetaBoostMultiplier += Value.SpinbackThetaBoostMultiplier * Weight;
        }
    } // namespace

    FString ShotRegimeKey::Build(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm)
    {
        return FString::Printf(TEXT("%s-%s-%s-%s"), GetFamily(SpeedMph, LaunchAngleDeg), GetSpeedBin(SpeedMph), GetLaunchBin(LaunchAngleDeg),
            GetSpinBin(TotalSpinRpm));
    }

    TArray<FString> ShotRegimeKey::BuildLookupKeys(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm)
    {
        const TCHAR* Family = GetFamily(SpeedMph, LaunchAngleDeg);
        const TCHAR* Speed = GetSpeedBin(SpeedMph);
        const TCHAR* Launch = GetLaunchBin(LaunchAngleDeg);
        return {FString::Printf(TEXT("%s-%s-%s-%s"), Family, Speed, Launch, GetSpinBin(TotalSpinRpm)),
            FString::Printf(TEXT("%s-%s-%s"), Family, Speed, Launch), FString::Printf(TEXT("%s-%s"), Family, Speed), FString(Family)};
    }

    const TCHAR* ShotRegimeKey::GetFamily(float SpeedMph, float LaunchAngleDeg)
    {
        if (SpeedMph < 60.0f)
        {
            return TEXT("C");
        }
        if (SpeedMph > 110.0f && LaunchAngleDeg < 18.0f)
        {
            return TEXT("D");
        }
        return LaunchAngleDeg > 30.0f ? TEXT("W") : TEXT("I");
    }

    const TCHAR* ShotRegimeKey::GetSpeedBin(float SpeedMph)
    {
        return SpeedMph < 60.0f ? TEXT("S0") : SpeedMph < 72.0f ? TEXT("S1a")
                                           : SpeedMph < 85.0f   ? TEXT("S1b")
                                           : SpeedMph < 105.0f  ? TEXT("S2")
                                           : SpeedMph < 120.0f  ? TEXT("S3")
                                                                : TEXT("S4");
    }

    const TCHAR* ShotRegimeKey::GetLaunchBin(float LaunchAngleDeg)
    {
        return LaunchAngleDeg < 10.0f ? TEXT("V0") : LaunchAngleDeg < 18.0f ? TEXT("V1")
                                                 : LaunchAngleDeg < 25.0f   ? TEXT("V2")
                                                 : LaunchAngleDeg < 33.0f   ? TEXT("V3")
                                                                            : TEXT("V4");
    }

    const TCHAR* ShotRegimeKey::GetSpinBin(float TotalSpinRpm)
    {
        return TotalSpinRpm < 2500.0f ? TEXT("P0") : TotalSpinRpm < 4000.0f ? TEXT("P1")
                                                 : TotalSpinRpm < 5500.0f   ? TEXT("P2")
                                                 : TotalSpinRpm < 7500.0f   ? TEXT("P3")
                                                                            : TEXT("P4");
    }

    ELandingBucket LandingBucket::Classify(float LaunchAngleDeg, float TotalSpinRpm)
    {
        const FString Spin = ShotRegimeKey::GetSpinBin(TotalSpinRpm);
        if (Spin == TEXT("P0") || Spin == TEXT("P1"))
        {
            return ELandingBucket::Run;
        }
        if (Spin == TEXT("P2"))
        {
            return ELandingBucket::Release;
        }
        if (Spin == TEXT("P3"))
        {
            return ELandingBucket::Check;
        }
        const FString Launch = ShotRegimeKey::GetLaunchBin(LaunchAngleDeg);
        return Launch == TEXT("V0") || Launch == TEXT("V1") ? ELandingBucket::Check : ELandingBucket::Spinback;
    }

    const TCHAR* LandingBucket::GetName(ELandingBucket Bucket)
    {
        switch (Bucket)
        {
        case ELandingBucket::Release:
            return TEXT("Release");
        case ELandingBucket::Check:
            return TEXT("Check");
        case ELandingBucket::Spinback:
            return TEXT("Spinback");
        default:
            return TEXT("Run");
        }
    }

    FBallPhysicsProfile::FBallPhysicsProfile()
    {
        // First-impact bite per landing bucket. Run is neutral so drivers, woods and low-spin irons
        // keep the calibrated bounce. Bites fade in from 8 to 20 m/s impact speed.
        LandingResponses[static_cast<int32>(ELandingBucket::Run)] = FLandingResponse();
        LandingResponses[static_cast<int32>(ELandingBucket::Release)] = FLandingResponse{0.5f, 0.0f, 8.0f, 20.0f};
        LandingResponses[static_cast<int32>(ELandingBucket::Check)] = FLandingResponse{0.85f, 0.0f, 8.0f, 20.0f};
        LandingResponses[static_cast<int32>(ELandingBucket::Spinback)] = FLandingResponse{1.0f, 1.2f, 8.0f, 20.0f};

        // Calibrated regime overrides from FS reference data: they correct systematic carry
        // bias per launch regime (chips under-carry at low Reynolds, drivers over-carry at high).
        // Chip shots (speed < 60 mph).
        RegimeScaleOverrides.Add(TEXT("C-S0"), Scale(0.70f, 1.20f));
        RegimeScaleOverrides.Add(TEXT("C-S0-V1-P0"), Scale(0.55f, 1.15f));
        RegimeScaleOverrides.Add(TEXT("C-S0-V4-P3"), Scale(0.65f, 1.25f));
        // Slow iron S1a (60-72 mph).
        RegimeScaleOverrides.Add(TEXT("I-S1a-V0-P1"), Scale(0.94f, 1.04f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V2-P1"), Scale(0.80f, 1.14f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V2-P2"), Scale(0.82f, 1.13f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V2-P3"), Scale(0.82f, 1.12f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V3-P2"), Scale(0.79f, 1.14f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V3-P3"), Scale(0.94f, 1.04f));
        RegimeScaleOverrides.Add(TEXT("I-S1a-V1-P2"), Scale(0.92f, 1.05f));
        // Mid iron S1b (72-85 mph).
        RegimeScaleOverrides.Add(TEXT("I-S1b-V0-P0"), Scale(0.97f, 1.02f));
        RegimeScaleOverrides.Add(TEXT("I-S1b-V2-P2"), Scale(0.94f, 1.03f));
        RegimeScaleOverrides.Add(TEXT("I-S1b-V2-P3"), Scale(0.97f, 1.01f));
        RegimeScaleOverrides.Add(TEXT("I-S1b-V3-P2"), Scale(0.88f, 1.06f));
        RegimeScaleOverrides.Add(TEXT("I-S1b-V3-P3"), Scale(0.97f, 1.02f));
        RegimeScaleOverrides.Add(TEXT("I-S1b-V1-P2"), Scale(0.98f, 1.01f));
        // Wedge lob (launch > 30 deg, 60-72 mph).
        RegimeScaleOverrides.Add(TEXT("W-S1a-V3-P3"), Scale(0.83f, 1.10f));
        // Fast iron with high spin.
        RegimeScaleOverrides.Add(TEXT("I-S3-V2-P3"), Scale(1.11f, 0.94f));
        RegimeScaleOverrides.Add(TEXT("I-S3-V1-P2"), Scale(1.04f, 0.98f));
        RegimeScaleOverrides.Add(TEXT("I-S2-V2-P3"), Scale(1.05f));
        RegimeScaleOverrides.Add(TEXT("I-S2-V2-P4"), Scale(1.06f, 0.95f));
        // Mid-speed iron.
        RegimeScaleOverrides.Add(TEXT("I-S2-V1-P2"), Scale(1.03f, 0.99f));
        RegimeScaleOverrides.Add(TEXT("I-S2-V0-P2"), Scale(1.06f, 0.96f));
        RegimeScaleOverrides.Add(TEXT("I-S2-V1-P0"), Scale(0.96f, 1.03f));
        RegimeScaleOverrides.Add(TEXT("I-S2-V1-P1"), Scale(0.97f, 1.02f));
        // Driver regime.
        RegimeScaleOverrides.Add(TEXT("D-S3-V1"), Scale(1.04f, 0.99f));
        RegimeScaleOverrides.Add(TEXT("D-S4-V0-P1"), Scale(1.03f, 0.98f));
        RegimeScaleOverrides.Add(TEXT("D-S4-V0-P2"), Scale(1.09f, 0.94f));
        RegimeScaleOverrides.Add(TEXT("D-S4-V1-P0"), Scale(0.98f, 1.02f));
        RegimeScaleOverrides.Add(TEXT("D-S4-V1-P1"), Scale(1.04f));
        RegimeScaleOverrides.Add(TEXT("D-S4-V1-P2"), Scale(1.04f));
        // High-speed wedge (launch > 30 deg, 85-105 mph).
        RegimeScaleOverrides.Add(TEXT("W-S2-V3-P4"), Scale(1.06f, 0.97f));
    }

    FBallPhysicsProfile FBallPhysicsProfile::HistoricalReference()
    {
        FBallPhysicsProfile Profile;
        Profile.bInterpolateCalibration = false;
        Profile.Flight.SpinDecayCoefficient = 0.0f;
        Profile.Flight.ConstantSpinDecayRatePerSecond = 0.2f;
        Profile.Flight.LowLaunchLiftRecoveryMax = 1.08f;
        return Profile;
    }

    FRegimeScaleOverride FBallPhysicsProfile::ResolveScaleOverride(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm,
        FString& OutRegimeKey, FString& OutMatchedKey) const
    {
        OutRegimeKey = ShotRegimeKey::Build(SpeedMph, LaunchAngleDeg, TotalSpinRpm);
        if (!bInterpolateCalibration)
        {
            return LookupRegime(*this, SpeedMph, LaunchAngleDeg, TotalSpinRpm, OutMatchedKey);
        }
        OutMatchedKey = TEXT("interpolated");
        const FBlendAxis Speed = Bracket(SpeedAnchors, SpeedMph);
        const FBlendAxis Launch = Bracket(LaunchAnchors, LaunchAngleDeg);
        const FBlendAxis Spin = Bracket(SpinAnchors, TotalSpinRpm);
        FRegimeScaleOverride Result{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        FString Ignored;
        for (int32 S = 0; S < 2; ++S)
        {
            for (int32 V = 0; V < 2; ++V)
            {
                for (int32 P = 0; P < 2; ++P)
                {
                    const float Weight = Speed.Weights[S] * Launch.Weights[V] * Spin.Weights[P];
                    if (Weight > 0.0f)
                    {
                        Accumulate(Result, LookupRegime(*this, Speed.Values[S], Launch.Values[V], Spin.Values[P], Ignored), Weight);
                    }
                }
            }
        }
        return Result;
    }

    FLandingResponse FBallPhysicsProfile::ResolveLandingResponse(float LaunchAngleDeg, float TotalSpinRpm, ELandingBucket& OutBucket) const
    {
        OutBucket = LandingBucket::Classify(LaunchAngleDeg, TotalSpinRpm);
        if (!bInterpolateCalibration)
        {
            return LandingResponses[static_cast<int32>(OutBucket)];
        }
        const FBlendAxis Launch = Bracket(LaunchAnchors, LaunchAngleDeg);
        const FBlendAxis Spin = Bracket(SpinAnchors, TotalSpinRpm);
        FLandingResponse Result;
        for (int32 V = 0; V < 2; ++V)
        {
            for (int32 P = 0; P < 2; ++P)
            {
                const float Weight = Launch.Weights[V] * Spin.Weights[P];
                const FLandingResponse& Value = LandingResponses[static_cast<int32>(LandingBucket::Classify(Launch.Values[V], Spin.Values[P]))];
                Result.SpinBiteScale += Value.SpinBiteScale * Weight;
                Result.MaxSpinbackMps += Value.MaxSpinbackMps * Weight;
                Result.SpeedRampStartMps += Value.SpeedRampStartMps * Weight;
                Result.SpeedRampEndMps += Value.SpeedRampEndMps * Weight;
            }
        }
        return Result;
    }

    FResolvedShotParams FResolvedShotParams::Resolve(const FBallPhysicsProfile& Profile, const FAirConditions& Air, float SpeedMph,
        float LaunchAngleDeg, float TotalSpinRpm)
    {
        FResolvedShotParams Params;
        Params.Regime = Profile.ResolveScaleOverride(SpeedMph, LaunchAngleDeg, TotalSpinRpm, Params.RegimeKey, Params.MatchedRegimeKey);
        Params.Landing = Profile.ResolveLandingResponse(LaunchAngleDeg, TotalSpinRpm, Params.LandingBucket);
        const float TemperatureC = Atmosphere::FahrenheitToCelsius(Air.TemperatureF);
        Params.Environment.AirDensity = Atmosphere::Density(Air.AltitudeFt * FeetToMeters, TemperatureC);
        Params.Environment.AirViscosity = Atmosphere::DynamicViscosity(TemperatureC);
        Params.Environment.DragScale = Profile.DragScaleMultiplier * Params.Regime.DragScaleMultiplier;
        Params.Environment.LiftScale = Profile.LiftScaleMultiplier * Params.Regime.LiftScaleMultiplier;
        Params.Environment.InitialLaunchAngleDeg = LaunchAngleDeg;
        Params.Flight = Profile.Flight;
        Params.Bounce = Profile.Bounce;
        Params.Rollout = Profile.Rollout;
        return Params;
    }

    FSurfaceSettings FResolvedShotParams::ResolveSurface(ESurface Surface, const FBallPhysicsProfile& Profile) const
    {
        const FSurfacePhysicsSettings& Base = SurfaceCatalog::Get(Surface);
        // A green's rolling friction is its stimp speed, set on the profile.
        const float BaseRolling = Surface == ESurface::Green ? GreenSpeed::RollingFriction(Profile.GreenStimpFeet) : Base.RollingFriction;

        FSurfaceSettings Settings;
        Settings.KineticFriction = Base.KineticFriction * Profile.KineticFrictionMultiplier * Regime.KineticFrictionMultiplier;
        Settings.RollingFriction = BaseRolling * Profile.RollingFrictionMultiplier * Regime.RollingFrictionMultiplier;
        Settings.GrassViscosity = Base.GrassViscosity * Profile.GrassViscosityMultiplier * Regime.GrassViscosityMultiplier;
        Settings.SpinbackResponseScale = Base.SpinbackResponseScale;
        Settings.CriticalAngle = Base.CriticalAngle + Profile.CriticalAngleOffsetRadians + Regime.CriticalAngleOffsetRadians;
        Settings.SpinbackThetaBoostMax = Base.SpinbackThetaBoostMax * Profile.SpinbackThetaBoostMultiplier * Regime.SpinbackThetaBoostMultiplier;
        Settings.SpinbackSpinStartRpm = Base.SpinbackSpinStartRpm;
        Settings.SpinbackSpinEndRpm = Base.SpinbackSpinEndRpm;
        Settings.SpinbackSpeedStartMps = Base.SpinbackSpeedStartMps;
        Settings.SpinbackSpeedEndMps = Base.SpinbackSpeedEndMps;
        Settings.RollingSpinScale = Base.RollingSpinScale;
        Settings.RestitutionScale = Base.RestitutionScale;
        Settings.ImpactRetentionScale = Base.ImpactRetentionScale;
        Settings.SurfaceBounce = Base.SurfaceBounce;
        return Settings;
    }
} // namespace OpenFairway
