// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/ShotSetup.cs and PhysicsAdapter.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#pragma once

#include "OpenFairway/BallProfile.h"
#include "OpenFairway/GolfPhysics.h"

namespace OpenFairway
{
    constexpr float MpsPerMph = 0.44704f;
    constexpr float YardsPerMeter = 1.09361f;
    constexpr float FeetPerMeter = 3.28084f;

    /**
     * Launch-monitor ball data: mph, degrees and rpm with GSPro signs (positive HLA, axis and
     * side spin are to the right). Unset spin values are derived from the others.
     */
    struct FLaunchData
    {
        float SpeedMph = 0.0f;
        float VerticalLaunchDeg = 0.0f;
        float HorizontalLaunchDeg = 0.0f;
        TOptional<float> BackSpinRpm;
        TOptional<float> SideSpinRpm;
        TOptional<float> TotalSpinRpm;
        TOptional<float> SpinAxisDeg;

        /** False for non-finite values, negative speed or spin, or a launch angle outside +/-90 degrees. */
        OPENFAIRWAYPHYSICS_API bool IsValid() const;
    };

    /** Spin with every component filled in. */
    struct FShotSpin
    {
        float BackSpinRpm = 0.0f;
        float SideSpinRpm = 0.0f;
        float TotalSpinRpm = 0.0f;
        float SpinAxisDeg = 0.0f;
    };

    /** Initial physics vectors in the OpenFairway frame. */
    struct FLaunchVectors
    {
        FVector3f Velocity = FVector3f::ZeroVector;
        FVector3f Omega = FVector3f::ZeroVector;
        /** Normalized horizontal shot direction; distances are measured along it. */
        FVector3f ShotDirection = FVector3f(1.0f, 0.0f, 0.0f);
    };

    namespace ShotSetup
    {
        /** Normalizes BackSpin/SideSpin and TotalSpin/SpinAxis; measured components win when they disagree. */
        OPENFAIRWAYPHYSICS_API FShotSpin ParseSpin(const FLaunchData& Launch);

        /** Converts launch data to velocity and spin; a putt with no measured spin leaves the face rolling. */
        OPENFAIRWAYPHYSICS_API FLaunchVectors BuildLaunchVectors(float SpeedMph, float VerticalLaunchDeg, float HorizontalLaunchDeg, float BackSpinRpm,
            float SideSpinRpm);

        /** Right-handed rotation about a unit axis (Godot's Vector3.Rotated, which the reference used). */
        OPENFAIRWAYPHYSICS_API FVector3f Rotate(const FVector3f& Vector, const FVector3f& Axis, float AngleRadians);
    } // namespace ShotSetup

    /** Outcome of a headless shot on a flat surface. Distances are meters along the shot direction. */
    struct FShotSimulationResult
    {
        bool bValid = false;
        float CarryM = 0.0f;
        float TotalM = 0.0f;
        /** Offline distance at rest from the target line: positive is right. */
        float LateralM = 0.0f;
        float ApexM = 0.0f;
        float HangTimeS = 0.0f;
        float LandingSpeedMps = 0.0f;
        float LandingAngleDeg = 0.0f;
        /** False when the time limit ran out before the ball stopped. */
        bool bAtRest = false;
        ELandingBucket LandingBucket = ELandingBucket::Run;
        FString RegimeKey;
        FString MatchedRegimeKey;
        FShotSpin Spin;
    };

    /**
     * Headless shot simulation on one flat surface, stepping at 120 Hz like the ogolf-physics
     * PhysicsAdapter. Used for regression fixtures and to predict a shot without a world.
     */
    namespace ShotSimulator
    {
        constexpr float DefaultMaxTimeSeconds = 30.0f;

        OPENFAIRWAYPHYSICS_API FShotSimulationResult Simulate(const FLaunchData& Launch, ESurface Surface = ESurface::Fairway,
            const FBallPhysicsProfile& Profile = FBallPhysicsProfile(), const FAirConditions& Air = FAirConditions(),
            float MaxTimeSeconds = DefaultMaxTimeSeconds);
    } // namespace ShotSimulator
} // namespace OpenFairway
