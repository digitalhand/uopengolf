// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

class UOpenGolfTerrainDefinition;

/**
 * Distance from play (any region that is not rough) for many points in one area, up to a cap. It is built on the game
 * thread from a copy of the regions that can matter in that area; At then reads only that copy, so any thread can use
 * it, and it skips a region whose bounds are no nearer than the best distance found so far.
 */
class OPENGOLFCOURSE_API FOpenGolfPlayDistance
{
public:
    /** Copies the play regions whose bounds come within CapM of AreaM, both in terrain metres. */
    FOpenGolfPlayDistance(const UOpenGolfTerrainDefinition& Definition, const FBox2D& AreaM, double CapM);

    /**
     * Metres from PointM to the nearest play region, 0 inside one, or CapM when none is nearer. For points in the area,
     * this equals FMath::Min(Definition.DistanceFromPlay(PointM), CapM).
     */
    double At(FVector2D PointM) const;

    /** Play regions kept for the area. */
    int32 NumRegions() const { return Regions.Num(); }

    /** The terrain has play regions at all, near the area or not. Without any, DistanceFromPlay is infinite everywhere. */
    bool HasPlayRegions() const { return bHasPlayRegions; }

private:
    struct FRegion
    {
        FBox2D Bounds;
        TArray<FVector2D> OutlineM;
    };
    TArray<FRegion> Regions;
    double CapM = 0;
    bool bHasPlayRegions = false;
};
