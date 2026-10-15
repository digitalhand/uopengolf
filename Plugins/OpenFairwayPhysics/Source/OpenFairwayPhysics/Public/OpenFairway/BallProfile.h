// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/BallPhysicsProfile.cs, ShotRegimeKey.cs,
// LandingBucket.cs, RegimeScaleOverride.cs and PhysicsParamsFactory.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#pragma once

#include "OpenFairway/Surfaces.h"

namespace OpenFairway
{
    /** Launch-regime classifier shared by runtime physics and calibration: "<family>-<speed>-<launch>-<spin>". */
    namespace ShotRegimeKey
    {
        OPENFAIRWAYPHYSICS_API FString Build(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm);

        /** Most to least specific: full key, without spin, without launch, family only. */
        OPENFAIRWAYPHYSICS_API TArray<FString> BuildLookupKeys(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm);

        OPENFAIRWAYPHYSICS_API const TCHAR* GetFamily(float SpeedMph, float LaunchAngleDeg);
        OPENFAIRWAYPHYSICS_API const TCHAR* GetSpeedBin(float SpeedMph);
        OPENFAIRWAYPHYSICS_API const TCHAR* GetLaunchBin(float LaunchAngleDeg);
        OPENFAIRWAYPHYSICS_API const TCHAR* GetSpinBin(float TotalSpinRpm);
    } // namespace ShotRegimeKey

    /** How a shot is expected to behave after landing, from full release to spin-back. */
    enum class ELandingBucket : uint8
    {
        Run,
        Release,
        Check,
        Spinback,
        Count
    };

    namespace LandingBucket
    {
        /** Launch spin first, launch angle second; impact spin has decayed too far to tell a wedge from an iron. */
        OPENFAIRWAYPHYSICS_API ELandingBucket Classify(float LaunchAngleDeg, float TotalSpinRpm);

        OPENFAIRWAYPHYSICS_API const TCHAR* GetName(ELandingBucket Bucket);
    } // namespace LandingBucket

    /** Bounded multipliers and offsets applied for one launch regime. */
    struct FRegimeScaleOverride
    {
        float DragScaleMultiplier = 1.0f;
        float LiftScaleMultiplier = 1.0f;
        float KineticFrictionMultiplier = 1.0f;
        float RollingFrictionMultiplier = 1.0f;
        float GrassViscosityMultiplier = 1.0f;
        float CriticalAngleOffsetRadians = 0.0f;
        float SpinbackThetaBoostMultiplier = 1.0f;
    };

    /**
     * Ball-specific modifiers with the calibrated defaults: regime overrides derived from FS
     * reference data and landing responses per landing bucket.
     */
    struct OPENFAIRWAYPHYSICS_API FBallPhysicsProfile
    {
        float DragScaleMultiplier = 1.01f;
        float LiftScaleMultiplier = 1.0f;
        float KineticFrictionMultiplier = 1.0f;
        float RollingFrictionMultiplier = 1.0f;
        float GrassViscosityMultiplier = 1.0f;
        float CriticalAngleOffsetRadians = 0.0f;
        float SpinbackThetaBoostMultiplier = 1.0f;

        /** Speed of every green as a stimp reading in feet; a course property that travels with the profile. */
        float GreenStimpFeet = GreenSpeed::DefaultStimpFeet;

        TMap<FString, FRegimeScaleOverride> RegimeScaleOverrides;
        FLandingResponse LandingResponses[static_cast<int32>(ELandingBucket::Count)];

        FFlightProfile Flight;
        FBounceProfile Bounce;
        FRolloutProfile Rollout;

        /** The calibrated defaults (the C# BallPhysicsProfile constructor). */
        FBallPhysicsProfile();

        const FRegimeScaleOverride& ResolveScaleOverride(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm, FString& OutRegimeKey,
            FString& OutMatchedKey) const;

        const FLandingResponse& ResolveLandingResponse(float LaunchAngleDeg, float TotalSpinRpm, ELandingBucket& OutBucket) const;
    };

    /** Air conditions; defaults match the ogolf-physics headless simulator (75 F at sea level). */
    struct FAirConditions
    {
        float TemperatureF = 75.0f;
        float AltitudeFt = 0.0f;
    };

    /** Everything about a shot that is fixed at launch: air, scales, profiles and the landing response. */
    struct OPENFAIRWAYPHYSICS_API FResolvedShotParams
    {
        FFlightEnvironment Environment;
        FFlightProfile Flight;
        FBounceProfile Bounce;
        FRolloutProfile Rollout;
        FLandingResponse Landing;
        ELandingBucket LandingBucket = ELandingBucket::Run;
        FRegimeScaleOverride Regime;
        FString RegimeKey;
        FString MatchedRegimeKey;

        /** Resolves a launch (PhysicsParamsFactory.Create without the surface). */
        static FResolvedShotParams Resolve(const FBallPhysicsProfile& Profile, const FAirConditions& Air, float SpeedMph, float LaunchAngleDeg,
            float TotalSpinRpm);

        /** Contact parameters of a surface for this shot, with profile and regime multipliers applied. */
        FSurfaceSettings ResolveSurface(ESurface Surface, const FBallPhysicsProfile& Profile) const;
    };
} // namespace OpenFairway
