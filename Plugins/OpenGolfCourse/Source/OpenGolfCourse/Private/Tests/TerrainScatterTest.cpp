// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainScatterTest, "UOpenGolf.Course.Terrain.Scatter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace TerrainScatterTestDetail
{
    UOpenGolfTerrainDefinition* MakeDefinition()
    {
        auto* Definition = NewObject<UOpenGolfTerrainDefinition>();
        auto* Rough = NewObject<UOpenGolfSurfacePreset>();
        Rough->Surface = EOpenFairwaySurface::Rough;
        Definition->DefaultSurface = Rough;
        auto* Fairway = NewObject<UOpenGolfSurfacePreset>();
        Fairway->Surface = EOpenFairwaySurface::Fairway;
        FOpenGolfTerrainRegion Region;
        Region.Name = TEXT("Fairway");
        Region.Surface = Fairway;
        Region.OutlineM = {{-10, -20}, {400, -20}, {400, 20}, {-10, 20}};
        Definition->Regions.Add(Region);
        return Definition;
    }

    /** Flat, tilted ground: the normal leans 20 degrees toward +X everywhere. */
    bool TiltedGround(FVector2D P, FVector& Position, FVector& Normal)
    {
        Position = FVector(P.X * 100, P.Y * 100, 0);
        Normal = FVector(FMath::Sin(FMath::DegreesToRadians(20.0)), 0, FMath::Cos(FMath::DegreesToRadians(20.0)));
        return true;
    }

    FVector2D Meters(const FOpenGolfScatterPoint& Point)
    {
        return FVector2D(Point.Transform.GetLocation()) / 100;
    }
} // namespace TerrainScatterTestDetail

bool FTerrainScatterTest::RunTest(const FString& Parameters)
{
    using namespace TerrainScatterTestDetail;
    const UOpenGolfTerrainDefinition* Definition = MakeDefinition();
    auto* Pines = NewObject<UOpenGolfVegetationPreset>();
    Pines->Kind = EOpenGolfVegetationKind::Tree;
    Pines->SpacingM = 9;
    Pines->ScaleRange = FVector2D(0.8, 1.2);
    const TArray<FVector2D> Belt = {{-40, 60}, {420, 60}, {420, 200}, {-40, 200}};
    FString Error;
    TArray<FOpenGolfScatterPoint> First, Second;
    TestTrue(TEXT("Tree belt scatters"), Definition->Scatter(*Pines, &Belt, TiltedGround, First, Error));
    TestTrue(TEXT("Tree belt is populated"), First.Num() > 300);
    Definition->Scatter(*Pines, &Belt, TiltedGround, Second, Error);
    bool bSame = First.Num() == Second.Num();
    for (int32 I = 0; bSame && I < First.Num(); ++I)
        bSame = First[I].Transform.Equals(Second[I].Transform) && First[I].Seed == Second[I].Seed;
    TestTrue(TEXT("Placement is deterministic"), bSame);

    bool bInside = true, bUpright = true, bOnRough = true, bScaled = true;
    for (const auto& Point : First)
    {
        bInside &= UOpenGolfTerrainDefinition::Contains(Belt, Meters(Point));
        bUpright &= Point.Transform.GetRotation().GetUpVector().Equals(FVector::UpVector, 0.0001);
        bOnRough &= Definition->SurfaceAt(Meters(Point))->Surface == EOpenFairwaySurface::Rough;
        bScaled &= Point.Transform.GetScale3D().Z >= 0.8 - 0.0001 && Point.Transform.GetScale3D().Z <= 1.2 + 0.0001;
    }
    TestTrue(TEXT("Trees stay inside their area"), bInside);
    TestTrue(TEXT("Trees stand upright on a slope"), bUpright);
    TestTrue(TEXT("Trees stay on their surface"), bOnRough);
    TestTrue(TEXT("Tree scale stays in range"), bScaled);

    Pines->MinimumDistanceFromPlayM = 70;
    Pines->MaximumDistanceFromPlayM = 120;
    TArray<FOpenGolfScatterPoint> Banded;
    Definition->Scatter(*Pines, &Belt, TiltedGround, Banded, Error);
    bool bInBand = !Banded.IsEmpty();
    for (const auto& Point : Banded)
    {
        const double Distance = Definition->DistanceFromPlay(Meters(Point));
        bInBand &= Distance >= 70 && Distance <= 120;
    }
    TestTrue(TEXT("A distance band limits placement"), bInBand && Banded.Num() < First.Num());
    Pines->MinimumDistanceFromPlayM = Pines->MaximumDistanceFromPlayM = 0;

    Pines->MaximumSlopeDeg = 10;
    TArray<FOpenGolfScatterPoint> Steep;
    Definition->Scatter(*Pines, &Belt, TiltedGround, Steep, Error);
    TestEqual(TEXT("Slopes beyond the limit stay clear"), Steep.Num(), 0);
    Pines->MaximumSlopeDeg = 30;

    auto* Grass = NewObject<UOpenGolfVegetationPreset>();
    Grass->SpacingM = 4;
    TArray<FOpenGolfScatterPoint> Cover;
    TestTrue(TEXT("Ground cover scatters over the detailed area"), Definition->Scatter(*Grass, nullptr, TiltedGround, Cover, Error));
    bool bInDetail = !Cover.IsEmpty(), bFollowsSlope = true;
    for (const auto& Point : Cover)
    {
        const FVector2D P = Meters(Point);
        bInDetail &= P.X > Definition->DetailMinimumM.X && P.X < Definition->DetailMaximumM.X && P.Y > Definition->DetailMinimumM.Y && P.Y < Definition->DetailMaximumM.Y;
        bFollowsSlope &= Point.Transform.GetRotation().GetUpVector().Z < 0.95;
    }
    TestTrue(TEXT("Ground cover stays in the detailed area"), bInDetail);
    TestTrue(TEXT("Ground cover follows the slope"), bFollowsSlope);

    // A strip narrower than three spacing steps still gets a row of trees.
    Pines->SpacingM = 34;
    const TArray<FVector2D> Strip = {{20, 74}, {430, 74}, {430, 124}, {20, 124}};
    TArray<FOpenGolfScatterPoint> Sparse;
    Definition->Scatter(*Pines, &Strip, TiltedGround, Sparse, Error);
    TestTrue(TEXT("A narrow, sparse strip is populated"), Sparse.Num() >= 8);
    Pines->SpacingM = 9;

    TArray<FOpenGolfScatterPoint> NoGround;
    Definition->Scatter(*Pines, &Belt, [](FVector2D, FVector&, FVector&)
        { return false; }, NoGround, Error);
    TestEqual(TEXT("Nothing is placed without ground"), NoGround.Num(), 0);

    Grass->SpacingM = 0.5f;
    const TArray<FVector2D> Everywhere = {Definition->MinimumM, {Definition->MaximumM.X, Definition->MinimumM.Y}, Definition->MaximumM, {Definition->MinimumM.X, Definition->MaximumM.Y}};
    TArray<FOpenGolfScatterPoint> TooMany;
    TestFalse(TEXT("The instance budget is enforced"), Definition->Scatter(*Grass, &Everywhere, TiltedGround, TooMany, Error));
    TestTrue(TEXT("An over-budget scatter returns nothing"), TooMany.IsEmpty() && !Error.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainRandomScatterTest, "UOpenGolf.Course.Terrain.RandomScatter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerrainRandomScatterTest::RunTest(const FString& Parameters)
{
    using namespace TerrainScatterTestDetail;
    auto* Definition = MakeDefinition();
    auto* Trees = NewObject<UOpenGolfVegetationPreset>();
    TestTrue(TEXT("Existing presets keep grid placement"), Trees->PlacementPattern == EOpenGolfPlacementPattern::JitteredGrid);
    Trees->Kind = EOpenGolfVegetationKind::Tree;
    Trees->PlacementPattern = EOpenGolfPlacementPattern::RandomSeparated;
    Trees->SpacingM = 14;
    Trees->MinimumSeparationM = 9;
    Trees->MinimumDistanceFromPlayM = 8;
    Trees->MaximumDistanceFromPlayM = 35;
    const TArray<FVector2D> Outline = {{0, 15}, {160, 15}, {160, 65}, {0, 65}};
    TArray<FOpenGolfScatterPoint> First, Again, Changed;
    FString Error;
    TestTrue(TEXT("Random placement succeeds"), Definition->Scatter(*Trees, &Outline, TiltedGround, First, Error));
    TestTrue(TEXT("Near rough has trees"), First.Num() > 10);
    Definition->Scatter(*Trees, &Outline, TiltedGround, Again, Error);
    bool bSame = First.Num() == Again.Num(), bClear = true, bSeparated = true;
    for (int32 I = 0; I < First.Num(); ++I)
    {
        bSame &= Again.IsValidIndex(I) && First[I].Transform.Equals(Again[I].Transform) && First[I].Seed == Again[I].Seed;
        const FVector2D P = Meters(First[I]);
        const double Distance = Definition->DistanceFromPlay(P);
        bClear &= Definition->Contains(Outline, P) && Distance >= 8 && Distance <= 35 &&
                  Definition->SurfaceAt(P)->Surface == EOpenFairwaySurface::Rough &&
                  First[I].Transform.GetRotation().GetUpVector().Equals(FVector::UpVector, 0.0001);
        for (int32 J = 0; J < I; ++J)
            bSeparated &= (P - Meters(First[J])).Size() >= Trees->MinimumSeparationM - 0.0001;
    }
    TestTrue(TEXT("Same seed reproduces transforms and mesh seeds"), bSame);
    TestTrue(TEXT("Random trees respect surface, distance, outline and upright placement"), bClear);
    TestTrue(TEXT("Random trees keep their minimum separation"), bSeparated);
    ++Trees->Seed;
    Definition->Scatter(*Trees, &Outline, TiltedGround, Changed, Error);
    TestTrue(TEXT("A new seed changes placement"), !Changed.IsEmpty() && !First.IsEmpty() && !Changed[0].Transform.Equals(First[0].Transform));

    Trees->MinimumDistanceFromPlayM = Trees->MaximumDistanceFromPlayM = 0;
    Trees->SpacingM = 4;
    Trees->MinimumSeparationM = 3;
    const TArray<FVector2D> Narrow = {{0, 40}, {100, 40}, {100, 42}, {0, 42}};
    Definition->Scatter(*Trees, &Narrow, TiltedGround, Again, Error);
    TestTrue(TEXT("A narrow random area remains populated"), Again.Num() > 5);
    Trees->MaximumSlopeDeg = 10;
    Definition->Scatter(*Trees, &Outline, TiltedGround, Again, Error);
    TestEqual(TEXT("Random placement rejects steep ground"), Again.Num(), 0);
    Trees->MaximumSlopeDeg = 30;
    int32 Queries = 0;
    Definition->Scatter(*Trees, &Narrow, [&Queries](FVector2D, FVector&, FVector&)
        { ++Queries; return false; }, Again, Error);
    TestTrue(TEXT("Missing ground finishes with bounded work"), Again.IsEmpty() && Queries > 0 && Queries <= 24 * 13);
    Trees->MinimumSeparationM = 200;
    Definition->Scatter(*Trees, &Narrow, TiltedGround, Again, Error);
    TestEqual(TEXT("An overcrowded area stops retrying"), Again.Num(), 1);
    Trees->MinimumSeparationM = -1;
    TestFalse(TEXT("Negative separation is rejected"), Definition->Scatter(*Trees, &Outline, TiltedGround, Again, Error));
    TestTrue(TEXT("Invalid settings return an error and no points"), !Error.IsEmpty() && Again.IsEmpty());
    Trees->MinimumSeparationM = 1;
    Trees->SpacingM = 0.1f;
    TestFalse(TEXT("Random placement enforces the point budget before sampling"), Definition->Scatter(*Trees, &Outline, TiltedGround, Again, Error));
    return true;
}
#endif
