// SPDX-License-Identifier: MIT
// Cup capture, after ogolf game/hole/CupCapture.cs. The green is a solid surface, so the cup is never a
// physical hole: this decides when the ball drops, and the ball then animates into the cup. A ball too fast to
// drop crosses the open cup here instead (StepOverCup), which ogolf does not model.
#pragma once

#include "CoreMinimal.h"
#include "OpenFairway/Profiles.h"

namespace OpenFairway::CupCapture
{
    /** Regulation cup: 4.25 in (108 mm) across and at least 4 in (101.6 mm) deep. */
    inline constexpr float CupDiameterM = 0.10795f;
    inline constexpr float CupRadiusM = CupDiameterM * 0.5f;
    inline constexpr float CupDepthM = 0.1016f;

    /** How far below the surface a holed ball's top ends up. */
    inline constexpr float DropDepthM = 0.03f;

    /**
     * Fastest horizontal speed (m/s) that still drops when the line of motion passes OffsetM from the cup centre.
     * Lip-entry model (Holmes 1991): the ball drops if it falls one ball radius before reaching the far rim of the
     * chord it crosses, about 1.64 m/s dead centre and 0 at the rim.
     */
    OPENFAIRWAYPHYSICS_API float MaxCaptureSpeed(float OffsetM);

    /**
     * True when a grounded ball at BallPosition moving with Velocity is over the cup and slow enough for its line to
     * drop. A stopped ball over the cup is holed. OpenFairway frame (Y up), metres and m/s.
     */
    OPENFAIRWAYPHYSICS_API bool CapturesRolling(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup);

    /**
     * True when a grounded ball is over the cup but too fast for its line to drop, so it rolls over or lips out.
     * OutSpeed is its horizontal speed and OutMaxSpeed the fastest that would have dropped on that line.
     */
    OPENFAIRWAYPHYSICS_API bool CrossesTooFast(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup, float& OutSpeed, float& OutMaxSpeed);

    /** A descending ground contact inside the cup at green level is a dunk. */
    OPENFAIRWAYPHYSICS_API bool CapturesLanding(const FVector3f& ContactPosition, const FVector3f& Cup);

    /**
     * Where a holed ball's centre settles: on its entry side of the cup, as close to the centre as the flagstick
     * allows, with the whole ball below the surface.
     */
    OPENFAIRWAYPHYSICS_API FVector3f DropPoint(const FVector3f& BallPosition, const FVector3f& Cup, float PoleRadiusM);

    /**
     * The rim's edge, struck by a ball crossing the cup: restitution along the contact normal and Coulomb friction across
     * it. These inherited coefficients are uncalibrated defaults; numerical validation does not establish
     * their realism for a particular green.
     */
    inline constexpr float RimRestitution = 0.1f;
    inline constexpr float RimFriction = 0.3f;

    /** Longest step StepOverCup takes, so the rim contact resolves well inside one ball radius of travel. */
    inline constexpr float CrossingSubstepS = 0.00025f;

    /**
     * Seconds since a ball at BallPosition moving with Velocity crossed onto the cup along its horizontal line, the
     * point a crossing starts from; 0 when its centre is not over the cup or it is not moving.
     */
    OPENFAIRWAYPHYSICS_API float TimeSinceRimEntry(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup);

    enum class ECupCrossing : uint8
    {
        /** Still over the open cup. */
        OverCup,
        /** Its centre fell below the rim inside the cup: the wall drives it in. */
        Holed,
        /** Back on or above the green outside the rim. */
        Out,
    };

    struct FCrossingStep
    {
        ECupCrossing Outcome = ECupCrossing::OverCup;
        float ConsumedSeconds = 0.0f;
    };

    /** Local cup frame (+Y surface normal); gravity must be rotated into that frame too.
     * Resolves contact at impact time and reports time used so callers can integrate the remainder after exit.
     * MaxSubstep is exposed for convergence tests, not for tuning the lip's response.
     */
    OPENFAIRWAYPHYSICS_API FCrossingStep AdvanceCrossing(FBallState& State, const FVector3f& Cup,
        const FVector3f& Gravity, float Dt, float MaxSubstep = CrossingSubstepS);

    /**
     * Compatibility wrapper for a level cup. For oriented cups or time accounting use AdvanceCrossing.
     * Advances a ball crossing the open cup too fast to drop (CrossesTooFast) by up to Dt. Off the green it falls freely,
     * and the rim's edge (a level circle at Cup.Y) strikes it with RimRestitution and RimFriction against its spin: a
     * firm ball hops over the far lip slower, one off centre turns across the cup, and one just over the limit through
     * the centre can hit the back and drop. OpenFairway frame (Y up), metres, m/s and rad/s.
     */
    OPENFAIRWAYPHYSICS_API ECupCrossing StepOverCup(FBallState& State, const FVector3f& Cup, float Dt);
} // namespace OpenFairway::CupCapture
