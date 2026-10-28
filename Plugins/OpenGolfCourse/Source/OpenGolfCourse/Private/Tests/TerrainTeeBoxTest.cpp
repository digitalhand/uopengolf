// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "OpenGolfTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainTeeBoxTest, "UOpenGolf.Course.Terrain.TeeBox",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace TerrainTeeBoxTestDetail
{
    constexpr double Smallest = 0.25;

    /** Rough everywhere, the default 5 m square tee at the origin, and a broad mound across it. */
    UOpenGolfTerrainDefinition* MakeDefinition()
    {
        auto* D = NewObject<UOpenGolfTerrainDefinition>();
        D->DefaultSurface = NewObject<UOpenGolfSurfacePreset>();
        D->DetailMinimumM = FVector2D(-10, -10);
        D->DetailMaximumM = FVector2D(10, 10);
        FOpenGolfTerrainMound Mound;
        Mound.CenterM = FVector2D::ZeroVector;
        Mound.RadiusM = FVector2D(40, 40);
        Mound.HeightM = 1;
        D->Mounds.Add(Mound);
        return D;
    }

    UOpenGolfVegetationPreset* MakePreset()
    {
        auto* P = NewObject<UOpenGolfVegetationPreset>();
        P->PlacementPattern = EOpenGolfPlacementPattern::Tiled;
        // As the cut-turf presets: no clearance, so the turf runs up to the tee box's mown edge.
        P->BoundaryClearanceM = 0;
        for (const double Size : {1.0, 0.5, Smallest})
        {
            FOpenGolfGrassTile Tile;
            Tile.SizeM = Size;
            Tile.Meshes.Add(NewObject<UStaticMesh>());
            P->Tiles.Add(Tile);
        }
        return P;
    }

    bool FlatGround(FVector2D XY, FVector& Position, FVector& Normal)
    {
        Position = FVector(XY.X * 100, XY.Y * 100, 0);
        Normal = FVector::UpVector;
        return true;
    }

    FIntPoint SmallestCell(FVector2D XY)
    {
        return FIntPoint(FMath::FloorToInt(XY.X / Smallest), FMath::FloorToInt(XY.Y / Smallest));
    }

    /** Smallest-tile cells covered by the tiles, and whether every tile keeps off the tee box. */
    TSet<FIntPoint> Cover(const UOpenGolfTerrainDefinition& D, const UOpenGolfVegetationPreset& P, const TArray<FOpenGolfScatterPoint>& Tiles, bool& bClear)
    {
        TSet<FIntPoint> Covered;
        bClear = true;
        for (const auto& Point : Tiles)
        {
            const FVector2D Center = FVector2D(Point.Transform.GetLocation()) / 100;
            const double Half = P.Tiles[Point.Tile].SizeM * 0.5;
            const bool bSmallestTile = Point.Tile == P.Tiles.Num() - 1;
            // Larger tiles keep their whole footprint off the box; the smallest are judged at their centre.
            for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
                bClear &= D.TeeBoxDistance(bSmallestTile ? Center : Center + Corner * Half) >= -0.0001;
            const int32 Cells = FMath::RoundToInt(2 * Half / Smallest);
            const FIntPoint Origin = SmallestCell(Center - FVector2D(Half) + FVector2D(Smallest * 0.5));
            for (int32 Y = 0; Y < Cells; ++Y)
                for (int32 X = 0; X < Cells; ++X)
                    Covered.Add(Origin + FIntPoint(X, Y));
        }
        return Covered;
    }
} // namespace TerrainTeeBoxTestDetail

bool FTerrainTeeBoxTest::RunTest(const FString& Parameters)
{
    using namespace TerrainTeeBoxTestDetail;
    UOpenGolfTerrainDefinition* D = MakeDefinition();
    TestEqual(TEXT("The tee box defaults to the painted 5 m square"), D->LevelTeeHalfSizeM, FVector2D(2.5, 2.5));

    TestEqual(TEXT("Centre is half a side inside"), D->TeeBoxDistance(FVector2D::ZeroVector), -2.5, 1e-9);
    TestEqual(TEXT("An edge is zero"), D->TeeBoxDistance(FVector2D(2.5, 1)), 0.0, 1e-9);
    TestEqual(TEXT("Beside an edge measures straight out"), D->TeeBoxDistance(FVector2D(3, 0)), 0.5, 1e-9);
    TestEqual(TEXT("Beyond a corner measures to the corner"), D->TeeBoxDistance(FVector2D(3, 3)), FMath::Sqrt(0.5), 1e-9);
    TestEqual(TEXT("Inside a corner is negative"), D->TeeBoxDistance(FVector2D(2.4, 2.4)), -0.1, 1e-9);

    TestEqual(TEXT("The tee centre is level"), D->HeightAt(FVector2D::ZeroVector), 0.0, 1e-9);
    TestEqual(TEXT("The tee box corners are level"), D->HeightAt(FVector2D(2.45, -2.45)), 0.0, 1e-9);
    const double Ramp = D->HeightAt(FVector2D(3.75, 0));
    TestTrue(TEXT("The ground eases back outside the box"), Ramp > 0 && Ramp < D->Mounds[0].HeightM);
    const double Contour = D->Mounds[0].HeightM * FMath::Exp(-FMath::Square(5.1 / 40) * 2.0);
    TestEqual(TEXT("Contours return one half-side out"), D->HeightAt(FVector2D(5.1, 0)), Contour, 1e-9);

    UOpenGolfVegetationPreset* P = MakePreset();
    FString Error;
    TArray<FOpenGolfScatterPoint> Tiles;
    TestTrue(TEXT("Tiled fill succeeds"), D->Scatter(*P, nullptr, FlatGround, Tiles, Error));
    bool bClear = false;
    const TSet<FIntPoint> Covered = Cover(*D, *P, Tiles, bClear);
    TestTrue(TEXT("No tile reaches into the tee box"), bClear);
    // A round clearance left bare grass beside each edge; the square one stops at the edge.
    for (const FVector2D Outside : {FVector2D(2.6, 0), FVector2D(-2.6, 0), FVector2D(0, 2.6), FVector2D(0, -2.6), FVector2D(2.6, 2.6)})
        TestTrue(*FString::Printf(TEXT("Grass reaches the tee box edge at %s"), *Outside.ToString()), Covered.Contains(SmallestCell(Outside)));
    for (const FVector2D Inside : {FVector2D(2.4, 0), FVector2D(0, -2.4), FVector2D(-2.4, 2.4)})
        TestFalse(*FString::Printf(TEXT("No grass inside the tee box at %s"), *Inside.ToString()), Covered.Contains(SmallestCell(Inside)));

    D->LevelTeeHalfSizeM = FVector2D::ZeroVector;
    TestEqual(TEXT("A zero box disables levelling"), D->HeightAt(FVector2D::ZeroVector), 1.0, 1e-9);
    TArray<FOpenGolfScatterPoint> Unlevelled;
    D->Scatter(*P, nullptr, FlatGround, Unlevelled, Error);
    TestTrue(TEXT("A zero box keeps no clearance"), Cover(*D, *P, Unlevelled, bClear).Contains(SmallestCell(FVector2D::ZeroVector)));

    auto* Bounded = NewObject<UOpenGolfTerrainDefinition>();
    Bounded->DefaultSurface = D->DefaultSurface;
    TestTrue(TEXT("The default tee box is valid"), Bounded->Validate(Error));
    Bounded->LevelTeeHalfSizeM = FVector2D(-1, 2.5);
    TestFalse(TEXT("A negative tee box is rejected"), Bounded->Validate(Error));
    return true;
}
#endif
