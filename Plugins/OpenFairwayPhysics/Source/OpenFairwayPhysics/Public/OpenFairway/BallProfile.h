// SPDX-License-Identifier: MIT
// The ball profile, landing buckets and shot parameters; the landing buckets were originally ported from
// ogolf-physics (LandingBucket.cs, PhysicsParamsFactory.cs).
#pragma once

#include "OpenFairway/Surfaces.h"
#include "OpenFairway/WindProfile.h"

namespace OpenFairway
{
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
        /**
         * The bucket of a launch: Run below 4000 rpm of backspin, Release below 5500, Check below 7500, and above
         * that Check for a launch under 18 degrees, else Spinback. Descriptive: the response a shot gets is
         * interpolated across launch angle and spin (FBallPhysicsProfile::ResolveLandingResponse).
         */
        OPENFAIRWAYPHYSICS_API ELandingBucket Classify(float LaunchAngleDeg, float TotalSpinRpm);

        OPENFAIRWAYPHYSICS_API const TCHAR* GetName(ELandingBucket Bucket);
    } // namespace LandingBucket

    /** The ball's physics: flight, bounce and roll-out constants, surface multipliers and the landing responses. */
    struct OPENFAIRWAYPHYSICS_API FBallPhysicsProfile
    {
        /** Scales the air drag of a ball moving along the ground; flight ignores it. */
        float GroundDragScale = 1.01f;
        // Multipliers and offsets on every surface's contact values (FResolvedShotParams::ResolveSurface).
        float KineticFrictionMultiplier = 1.0f;
        float RollingFrictionMultiplier = 1.0f;
        float GrassViscosityMultiplier = 1.0f;
        float CriticalAngleOffsetRadians = 0.0f;
        float SpinbackThetaBoostMultiplier = 1.0f;

        /** Speed of every green as a stimp reading in feet; a course property that travels with the profile. */
        float GreenStimpFeet = GreenSpeed::DefaultStimpFeet;
        /** Speed-dependent grass drag of every green; the stimp's rolling friction is solved with it. */
        float GreenGrassViscosity = GreenSpeed::GrassViscosity;
        /** Forward speed a sand impact removes per m/s the ball drives into the sand, so a shallow full-shot landing stays near its pitch mark. */
        float SandPlowRatio = 2.5f;
        /**
         * Rolling friction in sand, in place of the catalog value (0.30), so a ball that rolls into a bunker
         * at 4 m/s stops within 2 m instead of running on 2.5 m.
         */
        float SandRollingFriction = 0.45f;

        /** First-impact bite per landing bucket, interpolated across launch angle and spin. */
        FLandingResponse LandingResponses[static_cast<int32>(ELandingBucket::Count)];

        FFlightProfile Flight;
        FBounceProfile Bounce;
        FRolloutProfile Rollout;

        /** The gameplay ball: spin-ratio flight, recalibrated turf and the landing responses. */
        FBallPhysicsProfile();

        /** The first-impact bite of a launch, blended between the buckets of the neighbouring launch angles and spins. */
        FLandingResponse ResolveLandingResponse(float LaunchAngleDeg, float TotalSpinRpm, ELandingBucket& OutBucket) const;
    };

    /** Air conditions; the defaults are 75 F at sea level, dry and calm. */
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
        /** Height (m) at which WindSpeedMph is measured: the weather-report height. 0 keeps the wind uniform. */
        float WindReferenceHeightM = WindProfile::StandardReferenceHeightM;
        /** Roughness length (m) of the ground the wind blows over. */
        float WindRoughnessM = WindProfile::OpenGrassRoughnessM;

        /** Air density in kg/m^3. */
        OPENFAIRWAYPHYSICS_API float Density() const;

        /** Wind velocity at the reference height in the OpenFairway frame (+X downrange, +Y up, +Z right), m/s. */
        OPENFAIRWAYPHYSICS_API FVector3f WindVelocity() const;

        /** Wind velocity at HeightM above the ground, m/s. */
        OPENFAIRWAYPHYSICS_API FVector3f WindVelocityAt(float HeightM) const;

        /** The air as a flight environment: density, viscosity and wind (ground drag scale 1). */
        OPENFAIRWAYPHYSICS_API FFlightEnvironment ToEnvironment() const;
    };

    /** Everything about a shot that is fixed at launch: air, profiles and the landing response. */
    struct OPENFAIRWAYPHYSICS_API FResolvedShotParams
    {
        FFlightEnvironment Environment;
        FFlightProfile Flight;
        FBounceProfile Bounce;
        FRolloutProfile Rollout;
        FLandingResponse Landing;
        ELandingBucket LandingBucket = ELandingBucket::Run;

        /** Resolves a launch. The landing response reads BackSpinRpm when it is given (only backspin bites), else the total spin. */
        static FResolvedShotParams Resolve(const FBallPhysicsProfile& Profile, const FAirConditions& Air, float LaunchAngleDeg, float TotalSpinRpm,
            TOptional<float> BackSpinRpm = {});

        /** Contact parameters of a surface for this shot, with the profile's multipliers applied. */
        FSurfaceSettings ResolveSurface(ESurface Surface, const FBallPhysicsProfile& Profile) const;
    };
} // namespace OpenFairway
