// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairway/BallProfile.h"
#include "OpenFairway/GolfPhysics.h"

/**
 * The pieces of a shot step that the headless simulator (ShotSimulator) and the ball actor (AOpenFairwayBall) share, so
 * the two play a shot the same way: one finds contacts on a plane, the other with Chaos sweeps, and both resolve them here.
 * OpenFairway frame, SI units.
 */
namespace OpenFairway::ShotStep
{
    /** The ball after an impact, and whether it rebounds high enough to bounce rather than roll. */
    struct FImpactOutcome
    {
        FVector3f Velocity = FVector3f::ZeroVector;
        FVector3f Spin = FVector3f::ZeroVector;
        bool bRebounds = false;
    };

    /** Resolves an impact with the surface (GolfPhysics::ResolveImpact) and tests the rebound (GolfPhysics::HasRebound). */
    OPENFAIRWAYPHYSICS_API FImpactOutcome Impact(const FVector3f& Velocity, const FVector3f& Spin, const FVector3f& Normal,
        const FSurfaceSettings& Surface, const FResolvedShotParams& Params, bool bFirstImpact);

    /** A ball that stays on the surface: its velocity without the part along the unit Normal. */
    OPENFAIRWAYPHYSICS_API FVector3f Settle(const FVector3f& Velocity, const FVector3f& Normal);

    /** Angle (deg) between the velocity and the surface with this unit normal, the landing angle. */
    OPENFAIRWAYPHYSICS_API float ImpactAngleDeg(const FVector3f& Velocity, const FVector3f& Normal);

    /** The share of the struck-along-the-ground grass drag (GolfPhysics::StruckAlongGroundWeight) of a launch into the surface. */
    OPENFAIRWAYPHYSICS_API float StruckWeight(const FVector3f& Velocity, const FVector3f& Normal);

    /** The flight up to its contact with a plane, and how long that took. */
    struct FContactFlight
    {
        FBallState State;
        float TimeS = 0.0f;
    };

    /**
     * Flies State until it meets the plane Dot(Position, Normal) = PlaneHeight within Dt (GolfPhysics::FlightContactTime),
     * the wind read at heights along WindUp. A very low launch can complete its whole flight inside one step, so the
     * contact comes from the flight itself, not from a chord between steps.
     */
    OPENFAIRWAYPHYSICS_API FContactFlight FlyToContact(const FBallState& State, const FResolvedShotParams& Params, float Dt, const FVector3f& Normal,
        float PlaneHeight, const FVector3f& WindUp);
} // namespace OpenFairway::ShotStep
