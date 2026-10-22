// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "OpenGolfTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainTiledGrassTest, "UOpenGolf.Course.Terrain.TiledGrass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace TerrainTiledGrassTestDetail
{
    constexpr double Smallest = 0.25;

    /** A 6 x 4 m elliptical fairway in rough, with the level tee nearby. */
    UOpenGolfTerrainDefinition* MakeDefinition()
    {
        auto* D = NewObject<UOpenGolfTerrainDefinition>();
        D->DefaultSurface = NewObject<UOpenGolfSurfacePreset>();
        D->DetailMinimumM = FVector2D(-20, -20);
        D->DetailMaximumM = FVector2D(20, 20);
        D->LevelTeeCenterM = FVector2D(0, -8);
        D->LevelTeeRadiusM = 1;
        auto* Fairway = NewObject<UOpenGolfSurfacePreset>();
        Fairway->Surface = EOpenFairwaySurface::Fairway;
        FOpenGolfTerrainRegion Region;
        Region.Surface = Fairway;
        for (int32 I = 0; I < 48; ++I)
            Region.OutlineM.Emplace(6 * FMath::Cos(2 * PI * I / 48), 4 * FMath::Sin(2 * PI * I / 48));
        D->Regions.Add(Region);
        return D;
    }

    UOpenGolfVegetationPreset* MakePreset()
    {
        auto* P = NewObject<UOpenGolfVegetationPreset>();
        P->PlacementPattern = EOpenGolfPlacementPattern::Tiled;
        P->BoundaryClearanceM = 0.3;
        P->MaximumDistanceFromPlayM = 5;
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
} // namespace TerrainTiledGrassTestDetail

bool FTerrainTiledGrassTest::RunTest(const FString& Parameters)
{
    using namespace TerrainTiledGrassTestDetail;
    UOpenGolfTerrainDefinition* D = MakeDefinition();
    UOpenGolfVegetationPreset* P = MakePreset();
    FString Error;
    TArray<FOpenGolfScatterPoint> First, Again;
    TestTrue(TEXT("Tiled fill succeeds"), D->Scatter(*P, nullptr, FlatGround, First, Error));
    TestTrue(TEXT("The band is filled"), First.Num() > 100);
    D->Scatter(*P, nullptr, FlatGround, Again, Error);
    bool bSame = First.Num() == Again.Num();
    for (int32 I = 0; bSame && I < First.Num(); ++I)
        bSame = First[I].Transform.Equals(Again[I].Transform) && First[I].Seed == Again[I].Seed && First[I].Tile == Again[I].Tile;
    TestTrue(TEXT("The seed reproduces every tile"), bSame);

    // Footprints, as smallest-tile cells: none may repeat, and together they must cover the band.
    TSet<FIntPoint> Covered;
    bool bDisjoint = true, bClear = true, bQuarterTurns = true, bSunk = true, bAllSizes = true, bInBand = true;
    TArray<int32> PerSize = {0, 0, 0};
    for (const auto& Point : First)
    {
        const FVector2D Center = FVector2D(Point.Transform.GetLocation()) / 100;
        const double Half = P->Tiles[Point.Tile].SizeM * 0.5;
        ++PerSize[Point.Tile];
        const double Yaw = FMath::Fmod(FMath::Abs(Point.Transform.Rotator().Yaw) + 0.001, 90.0);
        bQuarterTurns &= Yaw < 0.01;
        bSunk &= FMath::IsNearlyEqual(Point.Transform.GetLocation().Z, -2.0, 0.001) && Point.Transform.GetScale3D().Equals(FVector::OneVector);
        bInBand &= D->DistanceFromPlay(Center) <= 5;
        const bool bSmallestTile = Point.Tile == P->Tiles.Num() - 1;
        for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
        {
            // Larger tiles keep their whole footprint clear; the smallest are judged at their centre.
            const FVector2D Check = bSmallestTile ? Center : Center + Corner * Half;
            bClear &= D->SurfaceAt(Check)->Surface == EOpenFairwaySurface::Rough &&
                      UOpenGolfTerrainDefinition::DistanceToOutline(D->Regions[0].OutlineM, Check) >= 0.3 - 0.0001 &&
                      (Check - D->LevelTeeCenterM).Size() >= D->LevelTeeRadiusM + 0.3 - 0.0001;
        }
        const int32 Cells = FMath::RoundToInt(2 * Half / Smallest);
        const FIntPoint Origin = SmallestCell(Center - FVector2D(Half) + FVector2D(Smallest * 0.5));
        for (int32 Y = 0; Y < Cells; ++Y)
            for (int32 X = 0; X < Cells; ++X)
            {
                bool bAlready = false;
                Covered.Add(Origin + FIntPoint(X, Y), &bAlready);
                bDisjoint &= !bAlready;
            }
    }
    for (const int32 Count : PerSize)
        bAllSizes &= Count > 0;
    TestTrue(TEXT("Tiles never overlap"), bDisjoint);
    TestTrue(TEXT("Tiles keep clear of play and the tee"), bClear);
    TestTrue(TEXT("Tiles turn in quarter turns"), bQuarterTurns);
    TestTrue(TEXT("Tiles sit 2 cm into the ground at full size"), bSunk);
    TestTrue(TEXT("Edges use every tile size"), bAllSizes);
    TestTrue(TEXT("Tiles stay in the distance band"), bInBand);
    int32 Samples = 0, Missed = 0;
    for (double Y = -12; Y <= 12; Y += 0.1)
        for (double X = -12; X <= 12; X += 0.1)
        {
            const FVector2D XY(X, Y);
            const double FromPlay = D->DistanceFromPlay(XY);
            // The band is judged at tile centres, so its outer edge is ragged by up to half a tile diagonal.
            if (FromPlay < 0.55 || FromPlay > 4.25 || (XY - D->LevelTeeCenterM).Size() < D->LevelTeeRadiusM + 0.55)
                continue;
            ++Samples;
            Missed += Covered.Contains(SmallestCell(XY)) ? 0 : 1;
        }
    TestTrue(TEXT("The band has samples"), Samples > 1000);
    TestTrue(FString::Printf(TEXT("Tiles cover the band (%d of %d samples missed)"), Missed, Samples), Missed <= Samples / 100);

    // Curved ground: a mound in the rough makes smaller tiles where a flat field had whole ones.
    int32 FlatSmall = 0, CurvedSmall = 0;
    for (const auto& Point : First)
        FlatSmall += Point.Tile > 0 && FVector2D::Distance(FVector2D(Point.Transform.GetLocation()) / 100, FVector2D(0, 7)) < 2 ? 1 : 0;
    FOpenGolfTerrainMound Mound;
    Mound.CenterM = FVector2D(0, 7);
    Mound.RadiusM = FVector2D(2, 2);
    Mound.HeightM = 0.8f;
    D->Mounds.Add(Mound);
    TArray<FOpenGolfScatterPoint> Curved;
    D->Scatter(*P, nullptr, FlatGround, Curved, Error);
    for (const auto& Point : Curved)
        CurvedSmall += Point.Tile > 0 && FVector2D::Distance(FVector2D(Point.Transform.GetLocation()) / 100, FVector2D(0, 7)) < 2 ? 1 : 0;
    TestTrue(TEXT("Curved ground halves tiles"), CurvedSmall > FlatSmall);
    D->Mounds.Reset();

    TArray<FOpenGolfScatterPoint> Turned;
    D->Scatter(*P, nullptr, FlatGround, Turned, Error, 30);
    bool bFollowsTerrain = !Turned.IsEmpty();
    for (const auto& Point : Turned)
        bFollowsTerrain &= FMath::Fmod(FMath::Fmod(Point.Transform.Rotator().Yaw - 30, 90.0) + 90.001, 90.0) < 0.01;
    TestTrue(TEXT("The grid turns with the terrain"), bFollowsTerrain);

    TArray<FOpenGolfScatterPoint> None;
    D->Scatter(*P, nullptr, [](FVector2D, FVector&, FVector&) { return false; }, None, Error);
    TestTrue(TEXT("Missing ground stays empty"), None.IsEmpty());
    P->Tiles[1].SizeM = 0.4f;
    TestFalse(TEXT("Sizes that do not halve are rejected"), D->Scatter(*P, nullptr, FlatGround, None, Error));
    P->Tiles = {P->Tiles[0]};
    P->Tiles[0].SizeM = 0.05f;
    P->MaximumDistanceFromPlayM = 0;
    TestFalse(TEXT("The tile budget is bounded"), D->Scatter(*P, nullptr, FlatGround, None, Error));
    TestTrue(TEXT("A failure returns no tiles"), None.IsEmpty() && !Error.IsEmpty());
    return true;
}
#endif
