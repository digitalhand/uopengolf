// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfTerrainTiles.h"

namespace TerrainTileTestDetail
{
    UOpenGolfSurfacePreset* Preset(EOpenFairwaySurface Surface)
    {
        auto* Result = NewObject<UOpenGolfSurfacePreset>();
        Result->Surface = Surface;
        return Result;
    }

    TArray<FVector2D> Circle(FVector2D Centre, double Radius, int32 Count = 40)
    {
        TArray<FVector2D> Points;
        for (int32 I = 0; I < Count; ++I)
        {
            const double A = 2 * PI * I / Count;
            Points.Add(Centre + FVector2D(FMath::Cos(A), FMath::Sin(A) * 0.8) * Radius);
        }
        return Points;
    }

    /** Course ground with a green straddling the edge x = 0 between squares (-1, -1) and (0, -1). */
    UOpenGolfTerrainDefinition* Course(UOpenGolfSurfacePreset*& Green)
    {
        auto* D = NewObject<UOpenGolfTerrainDefinition>();
        D->DefaultSurface = Preset(EOpenFairwaySurface::Rough);
        D->DetailSpacingM = 2;
        D->MinimumM = FVector2D(-64, -64);
        D->MaximumM = FVector2D(192, 128);
        D->ElevationOriginM = FVector2D(-64, -64);
        D->ElevationSpacingM = 1;
        D->ElevationSize = FIntPoint(257, 193);
        for (int32 Y = 0; Y < D->ElevationSize.Y; ++Y)
            for (int32 X = 0; X < D->ElevationSize.X; ++X)
                D->ElevationM.Add(0.02 * X + 0.5 * FMath::Sin(Y * 0.1));
        FOpenGolfTerrainRegion Fairway;
        Fairway.Name = TEXT("way/1");
        Fairway.Surface = Preset(EOpenFairwaySurface::Fairway);
        Fairway.SpacingM = 1;
        Fairway.OutlineM = Circle(FVector2D(-4, -34), 26);
        FOpenGolfTerrainRegion GreenRegion;
        GreenRegion.Name = TEXT("way/2");
        Green = GreenRegion.Surface = Preset(EOpenFairwaySurface::Green);
        GreenRegion.SpacingM = 0.5;
        GreenRegion.OutlineM = Circle(FVector2D(-0.63, -33.7), 12, 57);
        D->Regions = {Fairway, GreenRegion};
        return D;
    }

    TSet<FVector2D> EdgeVertices(const OpenGolfTerrainTiles::FTile& Tile, double X)
    {
        TSet<FVector2D> Result;
        for (const auto& T : Tile.Triangles)
            for (int32 I : {T.A, T.B, T.C})
                if (Tile.Points[I].X == X)
                    Result.Add(Tile.Points[I]);
        return Result;
    }
} // namespace TerrainTileTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainTileTest, "UOpenGolf.Course.Terrain.Tiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerrainTileTest::RunTest(const FString& Parameters)
{
    namespace Detail = TerrainTileTestDetail;
    UOpenGolfSurfacePreset* Green = nullptr;
    const auto* D = Detail::Course(Green);
    OpenGolfTerrainTiles::FTile West, East;
    FString Error;
    if (!TestTrue(TEXT("West square triangulates"), OpenGolfTerrainTiles::Triangulate(*D, {-1, -1}, West, Error)) ||
        !TestTrue(TEXT("East square triangulates"), OpenGolfTerrainTiles::Triangulate(*D, {0, -1}, East, Error)))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Each triangle has a surface"), West.Surfaces.Num(), West.Triangles.Num());

    const TSet<FVector2D> WestEdge = Detail::EdgeVertices(West, 0), EastEdge = Detail::EdgeVertices(East, 0);
    TestTrue(TEXT("The shared edge has vertices"), WestEdge.Num() > 128);
    TestEqual(TEXT("Both squares place the same vertices along their shared edge"), WestEdge.Num(), EastEdge.Num());
    TestTrue(TEXT("The shared edge vertices match exactly"), WestEdge.Difference(EastEdge).IsEmpty());

    const auto& Outline = D->Regions[1].OutlineM;
    double LongestGreenEdge = 0;
    int32 Crossing = 0, GreenTriangles = 0;
    for (const auto* Tile : {&West, &East})
        for (int32 I = 0; I < Tile->Triangles.Num(); ++I)
        {
            const auto& T = Tile->Triangles[I];
            int32 Inside = 0, Outside = 0;
            for (int32 V : {T.A, T.B, T.C})
            {
                const FVector2D P = Tile->Points[V];
                if (UOpenGolfTerrainDefinition::DistanceToOutline(Outline, P) < 1e-3)
                    continue;
                ++(UOpenGolfTerrainDefinition::Contains(Outline, P) ? Inside : Outside);
            }
            Crossing += Inside > 0 && Outside > 0;
            if (Tile->Surfaces[I] != Green)
                continue;
            ++GreenTriangles;
            for (const auto& [A, B] : {TPair<int32, int32>(T.A, T.B), {T.B, T.C}, {T.C, T.A}})
                LongestGreenEdge = FMath::Max(LongestGreenEdge, FVector2D::Distance(Tile->Points[A], Tile->Points[B]));
        }
    TestEqual(TEXT("No triangle crosses the green's edge"), Crossing, 0);
    TestTrue(TEXT("The green is covered"), GreenTriangles > 1500);
    TestTrue(FString::Printf(TEXT("Green triangles are at most 0.8 m across (longest %.2f m)"), LongestGreenEdge), LongestGreenEdge <= 0.8);
    TestTrue(FString::Printf(TEXT("A square stays within its vertex budget (%d)"), West.Points.Num()), West.Points.Num() < 20000);

    OpenGolfTerrainTiles::FTile Outside;
    TestFalse(TEXT("A square outside the elevation grid is refused"), OpenGolfTerrainTiles::Triangulate(*D, {5, 5}, Outside, Error));
    return true;
}
#endif
