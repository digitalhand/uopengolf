// SPDX-License-Identifier: MIT
#include "OpenGolfPlayDistance.h"

#include "OpenGolfTerrain.h"

FOpenGolfPlayDistance::FOpenGolfPlayDistance(const UOpenGolfTerrainDefinition& Definition, const FBox2D& AreaM, double InCapM)
    : CapM(InCapM)
{
    const FBox2D Reach = AreaM.ExpandBy(CapM);
    for (const FOpenGolfTerrainRegion& Region : Definition.Regions)
    {
        if (!Region.Surface || Region.Surface->Surface == EOpenFairwaySurface::Rough || Region.OutlineM.IsEmpty())
            continue;
        bHasPlayRegions = true;
        const FBox2D Bounds(Region.OutlineM);
        if (Bounds.Intersect(Reach))
            Regions.Add({Bounds, Region.OutlineM});
    }
}

double FOpenGolfPlayDistance::At(FVector2D PointM) const
{
    double Best = CapM;
    for (const FRegion& Region : Regions)
    {
        // The outline is never nearer than its bounds.
        if (Region.Bounds.ComputeSquaredDistanceToPoint(PointM) >= Best * Best)
            continue;
        Best = FMath::Min(Best, UOpenGolfTerrainDefinition::DistanceFromOutline(Region.OutlineM, PointM));
        if (Best <= 0)
            return 0;
    }
    return Best;
}
