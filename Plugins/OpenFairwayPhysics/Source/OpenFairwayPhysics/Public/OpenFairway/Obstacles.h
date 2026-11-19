// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/** How a ball meets what is not ground: upright obstacles (tree trunks), canopy (branches and leaves) and walls. OpenFairway frame (Y up). */
namespace OpenFairway::Obstacles
{
    /** Share of its speed a ball keeps off an obstacle. */
    constexpr float ObstacleRestitution = 0.30f;
    /** Least speed (m/s) a ball leaves any upward-facing obstacle contact with, so it cannot settle there. */
    constexpr float ObstacleEscapeMps = 1.0f;
    /** Share of its speed a ball keeps off a wall: any contact too steep to be ground. */
    constexpr float WallRestitution = 0.30f;
    /** Path (m) through canopy over which speed and spin fall by 1/e. */
    constexpr float CanopyDecayLengthM = 4.0f;
    /** Largest turn (deg) per square-root meter of canopy path. */
    constexpr float CanopyDeflectionDegPerSqrtM = 6.0f;

    /**
     * Velocity after hitting an obstacle. The ball reflects off the horizontal part of Normal and keeps ObstacleRestitution
     * of its speed. On any upward-facing contact it leaves at ObstacleEscapeMps or more: along the contact's horizontal
     * normal on a rim, or along AwayFromObstacle (its horizontal offset from the obstacle's axis) on a near-flat top.
     */
    OPENFAIRWAYPHYSICS_API FVector3f ResolveObstacleHit(const FVector3f& Velocity, const FVector3f& Normal, const FVector3f& AwayFromObstacle);

    /** Velocity after hitting a wall with this unit normal: a reflection keeping WallRestitution of the speed. */
    OPENFAIRWAYPHYSICS_API FVector3f ResolveWallHit(const FVector3f& Velocity, const FVector3f& Normal);

    /** Fraction of speed and spin kept after PathM meters of canopy: exp(-PathM / CanopyDecayLengthM). */
    OPENFAIRWAYPHYSICS_API float CanopySpeedFactor(float PathM);

    /**
     * Velocity after PathM meters through canopy. Speed scales by CanopySpeedFactor, so many short steps lose as much as
     * one long one. The direction turns by a seeded angle of at most CanopyDeflectionDegPerSqrtM * sqrt(PathM), so a shot
     * repeated with the same seeds takes the same path.
     */
    OPENFAIRWAYPHYSICS_API FVector3f ResolveCanopyDrag(const FVector3f& Velocity, float PathM, uint32 Seed);
} // namespace OpenFairway::Obstacles
