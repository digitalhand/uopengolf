// SPDX-License-Identifier: MIT
// Launch setup and the headless shot loop, originally ported from ogolf-physics (ShotSetup.cs, PhysicsAdapter.cs).
#pragma once

#include "OpenFairway/BallProfile.h"
#include "OpenFairway/GolfPhysics.h"
#include "OpenFairway/Units.h"

namespace OpenFairway
{
    /**
     * Launch-monitor ball data: mph, degrees and rpm, with positive HLA, axis and side spin
     * to the right. Unset spin values are derived from the others.
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

        /** Right-handed rotation of Vector about a unit axis by AngleRadians. */
        OPENFAIRWAYPHYSICS_API FVector3f Rotate(const FVector3f& Vector, const FVector3f& Axis, float AngleRadians);
    } // namespace ShotSetup

    /** Optional diagnostic sample, in core SI units; never used to drive physics. */
    struct FPhysicsTraceSample
    {
        float TimeS;
        float Dt;
        FString Phase;
        ESurface Surface;
        FBallState State;
        FVector3f Normal;
        float SlipMps;
        float EnergyJ;
    };
    using FPhysicsTrace = TFunction<void(const FPhysicsTraceSample&)>;
    inline void TracePhysics(const FPhysicsTrace& Trace, float TimeS, float Dt, const TCHAR* Phase,
        ESurface Surface, const FBallState& State, const FVector3f& Normal)
    {
        if (Trace)
        {
            const FVector3f Contact = State.Velocity + FVector3f::CrossProduct(State.Spin, -GolfPhysics::Radius * Normal);
            Trace({TimeS, Dt, Phase, Surface, State, Normal,
                (Contact - FVector3f::DotProduct(Contact, Normal) * Normal).Size(), GolfPhysics::KineticEnergy(State)});
        }
    }
    struct FShotSimulationOptions
    {
        float Dt = GolfPhysics::SimulationDt;
        FPhysicsTrace Trace;
        /** Explicit surface for calibration experiments; unset for gameplay predictions. */
        TOptional<FSurfaceSettings> SurfaceOverride;
    };

    /** Outcome of a headless shot on one planar surface. Distances are meters along the shot direction. */
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
        /** Spin at the first impact; 0 when the ball never lands. */
        float LandingSpinRpm = 0.0f;
        /** Speed along the ground just before the first impact; 0 when the ball never lands. */
        float FirstImpactTangentInMps = 0.0f;
        /** Speed along the ground just after the first impact, negative when the ball spins back. */
        float FirstImpactTangentOutMps = 0.0f;
        /** True when the first impact reversed the ball's direction along the ground. */
        bool bFirstImpactSpinback = false;
        /** False when the time limit ran out before the ball stopped. */
        bool bAtRest = false;
        int32 BounceCount = 0;
        float GroundDistanceM = 0.0f;
        ELandingBucket LandingBucket = ELandingBucket::Run;
        FShotSpin Spin;
        /** Seconds of flight outside the Reynolds number and spin ratio the flight table was fitted over. */
        float OutsideMeasuredCoverageS = 0;
        float MinFlightRe = MAX_flt;
        float MaxFlightRe = 0;
        float MinFlightSpinRatio = MAX_flt;
        float MaxFlightSpinRatio = 0;
    };

    /**
     * Headless shot simulation on one planar surface, stepping at 120 Hz like the ball actor. Used by the tests, the
     * shot-library replay and AOpenFairwayBall::PredictShot to play a shot without a world.
     * GroundNormal tilts the plane through the launch point (OpenFairway frame, +Y up); a zero
     * normal plays as level ground.
     */
    namespace ShotSimulator
    {
        // Fast ground shots on a green can legitimately roll beyond 30 seconds.
        constexpr float DefaultMaxTimeSeconds = 120.0f;

        OPENFAIRWAYPHYSICS_API FShotSimulationResult Simulate(const FLaunchData& Launch, ESurface Surface = ESurface::Fairway,
            const FBallPhysicsProfile& Profile = FBallPhysicsProfile(), const FAirConditions& Air = FAirConditions(),
            float MaxTimeSeconds = DefaultMaxTimeSeconds, const FVector3f& GroundNormal = FVector3f(0.0f, 1.0f, 0.0f),
            const FShotSimulationOptions& Options = FShotSimulationOptions());
    } // namespace ShotSimulator
} // namespace OpenFairway
