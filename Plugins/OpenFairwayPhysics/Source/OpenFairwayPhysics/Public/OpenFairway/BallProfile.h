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
        /** Descriptive launch category; the resolved response is interpolated independently. */
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
        /** Speed-dependent grass drag of every green; the stimp's rolling friction is solved with it. */
        float GreenGrassViscosity = GreenSpeed::GrassViscosity;
        /**
         * Forward speed a sand impact removes per m/s the ball drives into the sand, so a
         * shallow full-shot landing stays near its pitch mark. 0 is the ported hard-sand contact.
         */
        float SandPlowRatio = 2.5f;

        /** Interpolate calibrated scales and landing responses across launch conditions. */
        bool bInterpolateCalibration = true;

        TMap<FString, FRegimeScaleOverride> RegimeScaleOverrides;
        FLandingResponse LandingResponses[static_cast<int32>(ELandingBucket::Count)];

        FFlightProfile Flight;
        FBounceProfile Bounce;
        FRolloutProfile Rollout;

        /** Current defaults, with continuous calibration and speed-dependent spin decay. */
        FBallPhysicsProfile();

        /** Pinned C# behavior for reference verification; not the gameplay default. */
        static FBallPhysicsProfile HistoricalReference();

        /** Matched key is "interpolated" for continuous calibration; regime key remains descriptive. */
        FRegimeScaleOverride ResolveScaleOverride(float SpeedMph, float LaunchAngleDeg, float TotalSpinRpm, FString& OutRegimeKey,
            FString& OutMatchedKey) const;

        FLandingResponse ResolveLandingResponse(float LaunchAngleDeg, float TotalSpinRpm, ELandingBucket& OutBucket) const;
    };

    /** Air conditions; defaults match the ogolf-physics headless simulator (75 F at sea level, dry, calm). */
    struct FAirConditions
    {
        float TemperatureF = 75.0f;
        float AltitudeFt = 0.0f;
        /** 0-1. Humid air is less dense, so the ball carries slightly farther. */
        float RelativeHumidity = 0.0f;
        /** Measured station pressure; 0 derives it from AltitudeFt. */
        float PressurePa = 0.0f;
        float WindSpeedMph = 0.0f;
        /**
         * Direction the wind blows from, in degrees clockwise seen from above, measured from the +X
         * target line: 0 is a headwind, 90 comes from the player's right, 180 is a tailwind.
         */
        float WindFromDeg = 0.0f;

        /** Air density in kg/m^3. */
        OPENFAIRWAYPHYSICS_API float Density() const;

        /** Wind velocity in the OpenFairway frame (+X downrange, +Y up, +Z right), m/s. */
        OPENFAIRWAYPHYSICS_API FVector3f WindVelocity() const;
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
