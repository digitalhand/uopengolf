// SPDX-License-Identifier: MIT
// Finds a lie on the saved map's hole for tests that need a ball in the rough or a bunker.
#pragma once
#if WITH_DEV_AUTOMATION_TESTS

#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace LieSearch
{
    /**
     * The first spot on Surface, as the ball itself reads it, in a grid of StepCm over the box from Min to Max (world cm),
     * whose distance from the cup lies between MinCupM and MaxCupM. The ball is left wherever the search ended.
     */
    inline bool FindLie(AUOpenGolfGameMode& Mode, EOpenFairwaySurface Surface, const FVector2D& Min, const FVector2D& Max, double StepCm,
        double MinCupM, double MaxCupM, FVector& OutSpot)
    {
        AOpenFairwayBall& Ball = *Mode.GetBall();
        const AOpenGolfTerrainArea& Terrain = *Mode.GetRange()->Terrain;
        const FVector Cup = Mode.GetCupLocation();
        for (double X = Min.X; X <= Max.X; X += StepCm)
            for (double Y = Min.Y; Y <= Max.Y; Y += StepCm)
            {
                const double CupM = FVector2D::Distance(FVector2D(X, Y), FVector2D(Cup)) / 100.0;
                FVector Ground, Normal;
                if (CupM < MinCupM || CupM > MaxCupM || !Terrain.GroundAt(FVector(X, Y, Cup.Z), Ground, Normal))
                    continue;
                Ball.PlaceAt(Ground + FVector::UpVector * 10.0f);
                if (Ball.GetResult().Surface == Surface)
                {
                    OutSpot = Ground + FVector::UpVector * 10.0f;
                    return true;
                }
            }
        return false;
    }

    /** Rough between 100 m from the tee and 60 m short of the cup, within 60 m of the line, outside the short game. */
    inline bool FindApproachRough(AUOpenGolfGameMode& Mode, FVector& OutSpot)
    {
        const FVector Tee = Mode.GetRange()->GetTeeLocation(), Cup = Mode.GetCupLocation();
        return FindLie(Mode, EOpenFairwaySurface::Rough, FVector2D(Tee.X + 10000, Tee.Y - 6000), FVector2D(Cup.X - 6000, Tee.Y + 6000), 400,
            OpenGolfPlay::ShortGameDistanceM + 10.0, 1000.0, OutSpot);
    }

    /** Sand within the short game around the cup: a greenside bunker. */
    inline bool FindGreensideBunker(AUOpenGolfGameMode& Mode, FVector& OutSpot)
    {
        const FVector Cup = Mode.GetCupLocation();
        return FindLie(Mode, EOpenFairwaySurface::Sand, FVector2D(Cup.X - 2700, Cup.Y - 2700), FVector2D(Cup.X + 2700, Cup.Y + 2700), 100, 0.0,
            OpenGolfPlay::ShortGameDistanceM - 1.0, OutSpot);
    }
} // namespace LieSearch

#endif
