// SPDX-License-Identifier: MIT
// Landing buckets and shot parameters originally ported from ogolf-physics (LandingBucket.cs, PhysicsParamsFactory.cs).
#include "OpenFairway/BallProfile.h"

#include "OpenFairway/GolfPhysics.h"
#include "OpenFairway/Units.h"

namespace OpenFairway
{
    namespace BallProfileDetail
    {
        // Interpolation knots of the landing responses: interior samples of the bucket regions. Linear interpolation is
        // continuous at every knot and avoids concentrating a large change into a narrow boundary band.
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
    } // namespace BallProfileDetail

    ELandingBucket LandingBucket::Classify(float LaunchAngleDeg, float TotalSpinRpm)
    {
        if (TotalSpinRpm < 4000.0f)
        {
            return ELandingBucket::Run;
        }
        if (TotalSpinRpm < 5500.0f)
        {
            return ELandingBucket::Release;
        }
        if (TotalSpinRpm < 7500.0f)
        {
            return ELandingBucket::Check;
        }
        return LaunchAngleDeg < 18.0f ? ELandingBucket::Check : ELandingBucket::Spinback;
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
        // keep the calibrated bounce. Bites fade in from 8 to 20 m/s impact speed. Turf replaces the bite with its
        // own landing law (FSurfaceSettings::LandingAngleLoss), so the bite reaches only green and sand.
        LandingResponses[static_cast<int32>(ELandingBucket::Run)] = FLandingResponse();
        LandingResponses[static_cast<int32>(ELandingBucket::Release)] = FLandingResponse{0.5f, 0.0f, 8.0f, 20.0f};
        LandingResponses[static_cast<int32>(ELandingBucket::Check)] = FLandingResponse{0.85f, 0.0f, 8.0f, 20.0f};
        LandingResponses[static_cast<int32>(ELandingBucket::Spinback)] = FLandingResponse{1.0f, 1.2f, 8.0f, 20.0f};
    }

    FLandingResponse FBallPhysicsProfile::ResolveLandingResponse(float LaunchAngleDeg, float TotalSpinRpm, ELandingBucket& OutBucket) const
    {
        using namespace BallProfileDetail;
        OutBucket = LandingBucket::Classify(LaunchAngleDeg, TotalSpinRpm);
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

    float FAirConditions::Density() const
    {
        return Atmosphere::Density(AltitudeFt * MetersPerFoot, Atmosphere::FahrenheitToCelsius(TemperatureF), RelativeHumidity, PressurePa);
    }

    FVector3f FAirConditions::WindVelocity() const
    {
        // The wind blows away from WindFromDeg (clockwise from the +X target line toward +Z right).
        const float FromRadians = FMath::DegreesToRadians(WindFromDeg);
        return -WindSpeedMph * MpsPerMph * FVector3f(FMath::Cos(FromRadians), 0.0f, FMath::Sin(FromRadians));
    }

    FVector3f FAirConditions::WindVelocityAt(float HeightM) const
    {
        return WindProfile::At(WindVelocity(), HeightM, WindReferenceHeightM, WindRoughnessM);
    }

    FFlightEnvironment FAirConditions::ToEnvironment() const
    {
        FFlightEnvironment Environment;
        Environment.AirDensity = Density();
        Environment.AirViscosity = Atmosphere::DynamicViscosity(Atmosphere::FahrenheitToCelsius(TemperatureF));
        Environment.Wind = WindVelocity();
        Environment.WindReferenceHeightM = WindReferenceHeightM;
        Environment.WindRoughnessM = WindRoughnessM;
        return Environment;
    }

    FResolvedShotParams FResolvedShotParams::Resolve(const FBallPhysicsProfile& Profile, const FAirConditions& Air, float LaunchAngleDeg,
        float TotalSpinRpm, TOptional<float> BackSpinRpm)
    {
        FResolvedShotParams Params;
        // Only backspin bites at landing, so a sidespin-heavy launch is not bucketed as a checking wedge.
        const float LandingSpinRpm = BackSpinRpm.IsSet() ? FMath::Max(0.0f, BackSpinRpm.GetValue()) : TotalSpinRpm;
        Params.Landing = Profile.ResolveLandingResponse(LaunchAngleDeg, LandingSpinRpm, Params.LandingBucket);
        Params.Environment = Air.ToEnvironment();
        Params.Environment.GroundDragScale = Profile.GroundDragScale;
        Params.Flight = Profile.Flight;
        Params.Bounce = Profile.Bounce;
        Params.Rollout = Profile.Rollout;
        return Params;
    }

    FSurfaceSettings FResolvedShotParams::ResolveSurface(ESurface Surface, const FBallPhysicsProfile& Profile) const
    {
        const FSurfacePhysicsSettings& Base = SurfaceCatalog::Get(Surface);
        // A green's rolling friction is its stimp speed, solved with its grass drag; both are set on the profile, as are
        // the fairway's stimp and sand's rolling friction.
        const bool bGreen = Surface == ESurface::Green;
        const float BaseRolling = bGreen                          ? GreenSpeed::RollingFriction(Profile.GreenStimpFeet, Profile.GreenGrassViscosity)
                                  : Surface == ESurface::Fairway ? FairwaySpeed::RollingFriction(Profile.FairwayStimpFeet)
                                  : Surface == ESurface::Sand    ? FMath::Max(0.0f, Profile.SandRollingFriction)
                                                                 : Base.RollingFriction;
        const float BaseViscosity = bGreen ? Profile.GreenGrassViscosity : Base.GrassViscosity;

        FSurfaceSettings Settings;
        Settings.KineticFriction = Base.KineticFriction * Profile.KineticFrictionMultiplier;
        Settings.RollingFriction = BaseRolling * Profile.RollingFrictionMultiplier;
        Settings.GrassViscosity = BaseViscosity * Profile.GrassViscosityMultiplier;
        // Both grass drag terms scale together.
        Settings.GrassFormDrag = Base.GrassFormDrag * Profile.GrassViscosityMultiplier;
        Settings.SpinbackResponseScale = Base.SpinbackResponseScale;
        Settings.CriticalAngle = Base.CriticalAngle + Profile.CriticalAngleOffsetRadians;
        Settings.SpinbackThetaBoostMax = Base.SpinbackThetaBoostMax * Profile.SpinbackThetaBoostMultiplier;
        Settings.SpinbackSpinStartRpm = Base.SpinbackSpinStartRpm;
        Settings.SpinbackSpinEndRpm = Base.SpinbackSpinEndRpm;
        Settings.SpinbackSpeedStartMps = Base.SpinbackSpeedStartMps;
        Settings.SpinbackSpeedEndMps = Base.SpinbackSpeedEndMps;
        Settings.RollingSpinScale = Base.RollingSpinScale;
        Settings.SlidingSpinScale = Base.SlidingSpinScale;
        Settings.RestitutionScale = Base.RestitutionScale;
        Settings.ImpactRetentionScale = Base.ImpactRetentionScale;
        Settings.LandingAngleLoss = Base.LandingAngleLoss;
        Settings.LandingSpinLoss = Base.LandingSpinLoss;
        Settings.StruckViscosityScale = Base.StruckViscosityScale;
        Settings.StruckFormDragScale = Base.StruckFormDragScale;
        Settings.ImpactPlowRatio = Surface == ESurface::Sand ? FMath::Max(0.0f, Profile.SandPlowRatio) : 0.0f;
        Settings.SurfaceBounce = Base.SurfaceBounce;
        return Settings;
    }
} // namespace OpenFairway
