// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "IndexTypes.h"

class UOpenGolfSurfacePreset;
class UOpenGolfTerrainDefinition;

/**
 * The ground of an imported course is built in 64 m squares so one edit rebuilds only the squares it touches.
 * Each square is triangulated on its own, with every surface outline as a constrained edge, so surface borders
 * follow the outlines exactly and greens get their own fine spacing. Neighbouring squares place identical
 * vertices along their shared edge, so the ground and its collision have no gaps or T-junctions.
 */
namespace OpenGolfTerrainTiles
{
    constexpr double TileM = 64.0;
    /** Spacing of the vertices along every square's edge; outline crossings snap to it when closer than SnapM. */
    constexpr double EdgeSpacingM = 0.5;
    constexpr double SnapM = 0.1;

    struct FTile
    {
        TArray<FVector2D> Points;
        TArray<UE::Geometry::FIndex3i> Triangles;
        /** Surface of each triangle: the last region whose outline holds it, or the default surface. */
        TArray<UOpenGolfSurfacePreset*> Surfaces;
    };

    /** The square covering Tile's grid cell, in terrain-local metres. */
    OPENGOLFCOURSEEDITOR_API FBox2D Bounds(FIntPoint Tile);

    /** Triangulates one square. Region outlines must not cross; the course layout tools ensure that. */
    OPENGOLFCOURSEEDITOR_API bool Triangulate(const UOpenGolfTerrainDefinition& Definition, FIntPoint Tile, FTile& Out, FString& Error);
}
