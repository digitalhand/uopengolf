// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "OpenGolfTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainFirstCutTest, "UOpenGolf.Course.Terrain.FirstCut",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerrainFirstCutTest::RunTest(const FString& Parameters)
{
    auto* D = NewObject<UOpenGolfTerrainDefinition>();
    D->DefaultSurface = NewObject<UOpenGolfSurfacePreset>();
    D->DetailMinimumM = FVector2D(-8, -8);
    D->DetailMaximumM = FVector2D(8, 8);
    D->LevelTeeHalfSizeM = FVector2D::ZeroVector;
    auto AddRegion = [D](EOpenFairwaySurface Surface, double X)
    {
        FOpenGolfTerrainRegion R;
        R.Surface = NewObject<UOpenGolfSurfacePreset>();
        R.Surface->Surface = Surface;
        R.OutlineM = {FVector2D(X, -2), FVector2D(X + 2, -2), FVector2D(X + 2, 2), FVector2D(X, 2)};
        D->Regions.Add(R);
    };
    AddRegion(EOpenFairwaySurface::Fairway, -3);
    AddRegion(EOpenFairwaySurface::Sand, 3);
    TestEqual(TEXT("Inside fairway is zero"), D->DistanceFromSurface(FVector2D(-2, 0), EOpenFairwaySurface::Fairway), 0.0);
    TestEqual(TEXT("Band distance ignores nearby sand"), D->DistanceFromSurface(FVector2D(2.9, 0), EOpenFairwaySurface::Fairway), 3.9);
    auto* P = NewObject<UOpenGolfVegetationPreset>();
    P->PlacementPattern = EOpenGolfPlacementPattern::Tiled;
    P->BoundaryClearanceM = 0;
    P->bUseDistanceSurface = true;
    P->DistanceSurface = EOpenFairwaySurface::Fairway;
    P->MaximumDistanceFromPlayM = 1;
    for (double Size : {0.5, 0.25, 0.125, 0.0625})
    {
        FOpenGolfGrassTile Tile;
        Tile.SizeM = Size;
        Tile.Meshes.Add(NewObject<UStaticMesh>());
        P->Tiles.Add(Tile);
    }
    auto Ground = [](FVector2D XY, FVector& Position, FVector& Normal)
    {
        Position = FVector(XY.X * 100, XY.Y * 100, 0);
        Normal = FVector::UpVector;
        return true;
    };
    FString Error;
    TArray<FOpenGolfScatterPoint> First, Again;
    TestTrue(TEXT("First cut scatters"), D->Scatter(*P, nullptr, Ground, First, Error));
    TestTrue(TEXT("First cut has grass"), First.Num() > 0);
    TestTrue(TEXT("Second bake scatters"), D->Scatter(*P, nullptr, Ground, Again, Error));
    TestEqual(TEXT("Repeated population count"), Again.Num(), First.Num());
    for (int32 I = 0; I < First.Num(); ++I)
    {
        const FVector2D XY = FVector2D(First[I].Transform.GetLocation()) / 100;
        if (D->SurfaceAt(XY)->Surface != EOpenFairwaySurface::Rough || D->DistanceFromSurface(XY, EOpenFairwaySurface::Fairway) > 1)
        {
            AddError(TEXT("First cut crossed its surface or outer boundary"));
            break;
        }
        if (Again.IsValidIndex(I) && !First[I].Transform.Equals(Again[I].Transform))
        {
            AddError(TEXT("First cut changed on a repeated bake"));
            break;
        }
    }
    P->MaximumDistanceFromPlayM = 0;
    P->bUseDistanceSurface = false;
    P->FairwayClearanceM = 1;
    First.Reset();
    TestTrue(TEXT("Tall rough scatters"), D->Scatter(*P, nullptr, Ground, First, Error));
    for (const auto& Point : First)
        if (D->DistanceFromSurface(FVector2D(Point.Transform.GetLocation()) / 100, EOpenFairwaySurface::Fairway) < 1)
        {
            AddError(TEXT("Tall rough entered the first cut"));
            break;
        }
    return true;
}
#endif
