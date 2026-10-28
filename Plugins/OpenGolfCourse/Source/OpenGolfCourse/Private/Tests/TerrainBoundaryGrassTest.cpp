// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainBoundaryGrassTest, "UOpenGolf.Course.Terrain.BoundaryGrass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace TerrainBoundaryGrassTestDetail
{
    constexpr double Clearance = 0.15, Length = 1, Depth = 0.25;

    /** Counter-clockwise 6 x 4 m elliptical fairway and a clockwise bunker beside it, in rough. */
    UOpenGolfTerrainDefinition* MakeDefinition()
    {
        auto* D = NewObject<UOpenGolfTerrainDefinition>();
        D->DefaultSurface = NewObject<UOpenGolfSurfacePreset>();
        D->DetailMinimumM = FVector2D(-20, -20);
        D->DetailMaximumM = FVector2D(20, 20);
        D->LevelTeeCenterM = FVector2D(-15, -15);
        D->LevelTeeHalfSizeM = FVector2D(1, 1);
        auto* Fairway = NewObject<UOpenGolfSurfacePreset>();
        Fairway->Surface = EOpenFairwaySurface::Fairway;
        FOpenGolfTerrainRegion Region;
        Region.Surface = Fairway;
        for (int32 I = 0; I < 48; ++I)
            Region.OutlineM.Emplace(6 * FMath::Cos(2 * PI * I / 48), 4 * FMath::Sin(2 * PI * I / 48));
        D->Regions.Add(Region);
        auto* Sand = NewObject<UOpenGolfSurfacePreset>();
        Sand->Surface = EOpenFairwaySurface::Sand;
        Region.Surface = Sand;
        Region.OutlineM = {{9, 2}, {12, 2}, {12, -2}, {9, -2}};
        D->Regions.Add(Region);
        return D;
    }

    bool FlatGround(FVector2D XY, FVector& Position, FVector& Normal)
    {
        Position = FVector(XY.X * 100, XY.Y * 100, XY.X * 10);
        Normal = FVector(-0.1, 0, 1).GetSafeNormal();
        return true;
    }

    /** Whether a strip's footprint, turned to its heading, covers a point in terrain metres. */
    bool StripCovers(const FOpenGolfScatterPoint& Strip, FVector2D XY)
    {
        const FVector Forward = Strip.Transform.GetRotation().GetForwardVector();
        const FVector2D Along = FVector2D(Forward).GetSafeNormal(), Across(-Along.Y, Along.X);
        const FVector2D Offset = XY - FVector2D(Strip.Transform.GetLocation()) / 100;
        return FMath::Abs(FVector2D::DotProduct(Offset, Along)) <= Length * 0.5 + 0.001 &&
               FMath::Abs(FVector2D::DotProduct(Offset, Across)) <= Depth * 0.5 + 0.02;
    }

    /** Points every 5 cm along the line Offset metres outside a counter-clockwise outline that no strip covers. */
    int32 MissedAlong(const TArray<FVector2D>& Outline, const TArray<FOpenGolfScatterPoint>& Strips, double Offset, int32& Samples)
    {
        int32 Missed = 0;
        Samples = 0;
        for (int32 I = 0; I < Outline.Num(); ++I)
        {
            const FVector2D A = Outline[I], Edge = Outline[(I + 1) % Outline.Num()] - A;
            const FVector2D Out = FVector2D(Edge.Y, -Edge.X).GetSafeNormal();
            for (double T = 0; T < 1; T += 0.05 / Edge.Size())
            {
                const FVector2D Sample = A + Edge * T + Out * Offset;
                ++Samples;
                bool bCovered = false;
                for (const auto& Strip : Strips)
                    bCovered |= StripCovers(Strip, Sample);
                Missed += bCovered ? 0 : 1;
            }
        }
        return Missed;
    }
} // namespace TerrainBoundaryGrassTestDetail

bool FTerrainBoundaryGrassTest::RunTest(const FString& Parameters)
{
    using namespace TerrainBoundaryGrassTestDetail;
    UOpenGolfTerrainDefinition* D = MakeDefinition();
    auto* P = NewObject<UOpenGolfVegetationPreset>();
    P->BoundaryClearanceM = Clearance;
    P->EdgeStripSizeM = FVector2D(Length, Depth);
    P->ScaleRange = FVector2D(1, 1);
    FString Error;
    TArray<FOpenGolfScatterPoint> First, Again;
    TestTrue(TEXT("Boundary strips succeed"), D->ScatterBoundary(*P, FlatGround, First, Error));
    D->ScatterBoundary(*P, FlatGround, Again, Error);
    bool bSame = First.Num() == Again.Num();
    for (int32 I = 0; bSame && I < First.Num(); ++I)
        bSame = First[I].Transform.Equals(Again[I].Transform) && First[I].Seed == Again[I].Seed;
    TestTrue(TEXT("The seed reproduces every strip"), bSame);

    bool bOutside = true, bProjected = true, bAlongEdge = true, bFacesPlay = true;
    int32 NearBunker = 0;
    for (const auto& Strip : First)
    {
        const FVector2D XY = FVector2D(Strip.Transform.GetLocation()) / 100;
        const double FromPlay = D->DistanceFromPlay(XY);
        bOutside &= D->SurfaceAt(XY)->Surface == EOpenFairwaySurface::Rough && FromPlay >= Clearance + Depth * 0.4 &&
                    FromPlay <= Clearance + Depth * 0.5 + 0.05;
        // Undo the 2 cm sink along the ground normal to get back to the ground point.
        const FVector Up = Strip.Transform.GetRotation().GetUpVector();
        const FVector OnGround = Strip.Transform.GetLocation() + Up * 2;
        bProjected &= FMath::IsNearlyEqual(OnGround.Z, OnGround.X * 0.1, 0.001) && Up.X < -0.09;
        NearBunker += UOpenGolfTerrainDefinition::DistanceToOutline(D->Regions[1].OutlineM, XY) < 1 ? 1 : 0;
        // A graded strip is short at local +Y, so -Y must point into the rough whatever the outline's winding.
        const FVector2D Into = -FVector2D(Strip.Transform.GetRotation().GetRightVector()).GetSafeNormal();
        bFacesPlay &= D->DistanceFromPlay(XY + Into * 0.05) > D->DistanceFromPlay(XY - Into * 0.05);
        // The strip runs along the nearest edge, not across it.
        const FVector2D Nearest = XY / FVector2D(6, 4);
        if (UOpenGolfTerrainDefinition::DistanceToOutline(D->Regions[0].OutlineM, XY) < 1)
        {
            const FVector2D Tangent = FVector2D(-6 * Nearest.Y, 4 * Nearest.X).GetSafeNormal();
            bAlongEdge &= FMath::Abs(FVector2D::DotProduct(FVector2D(Strip.Transform.GetRotation().GetForwardVector()).GetSafeNormal(), Tangent)) > 0.95;
        }
    }
    TestTrue(TEXT("Strips sit just outside play, on rough"), bOutside);
    TestTrue(TEXT("Strips follow the ground and sink into it"), bProjected);
    TestTrue(TEXT("Strips run along the edge"), bAlongEdge);
    TestTrue(TEXT("A clockwise bunker is edged from outside"), NearBunker > 8);
    TestTrue(TEXT("Every strip's local +Y faces play"), bFacesPlay);

    // Continuity: every point along the strip line around the fairway lies on a strip.
    int32 Samples = 0;
    int32 Missed = MissedAlong(D->Regions[0].OutlineM, First, Clearance + Depth * 0.5, Samples);
    TestTrue(FString::Printf(TEXT("The edge row is continuous (%d of %d samples missed)"), Missed, Samples), Samples > 500 && Missed == 0);

    // Beyond a first cut: the row moves out by the fairway clearance and stays continuous.
    constexpr double FirstCut = 1;
    P->BoundaryClearanceM = 0;
    P->FairwayClearanceM = FirstCut;
    TArray<FOpenGolfScatterPoint> BeyondCut;
    TestTrue(TEXT("Strips beyond a first cut succeed"), D->ScatterBoundary(*P, FlatGround, BeyondCut, Error));
    bool bBeyondCut = true;
    for (const auto& Strip : BeyondCut)
        bBeyondCut &= D->DistanceFromSurface(FVector2D(Strip.Transform.GetLocation()) / 100, EOpenFairwaySurface::Fairway) >= FirstCut + Depth * 0.4;
    TestTrue(TEXT("Strips stay outside the first cut"), bBeyondCut);
    Missed = MissedAlong(D->Regions[0].OutlineM, BeyondCut, FirstCut + Depth * 0.5, Samples);
    TestTrue(FString::Printf(TEXT("The row beyond the first cut is continuous (%d of %d samples missed)"), Missed, Samples),
        Samples > 500 && Missed == 0);
    P->BoundaryClearanceM = Clearance;
    P->FairwayClearanceM = 0;

    TArray<FOpenGolfScatterPoint> Turned;
    D->ScatterBoundary(*P, FlatGround, Turned, Error, 90);
    TestTrue(TEXT("Strips turn with the terrain"), Turned.Num() == First.Num() &&
        FMath::Abs(FMath::Fmod(Turned[0].Transform.Rotator().Yaw - First[0].Transform.Rotator().Yaw + 360, 180.0) - 90) < 0.5);

    TArray<FOpenGolfScatterPoint> None;
    D->ScatterBoundary(*P, [](FVector2D, FVector&, FVector&) { return false; }, None, Error);
    TestTrue(TEXT("Missing ground stays empty"), None.IsEmpty());
    P->MaximumSlopeDeg = 0;
    D->ScatterBoundary(*P, FlatGround, None, Error);
    TestTrue(TEXT("Steep ground stays empty"), None.IsEmpty());
    P->MaximumSlopeDeg = 30;
    P->EdgeStripSizeM = FVector2D(0.01, Depth);
    TestFalse(TEXT("A tiny strip is rejected"), D->ScatterBoundary(*P, FlatGround, None, Error));
    TestTrue(TEXT("A failure returns no strips"), None.IsEmpty() && !Error.IsEmpty());
    P->EdgeStripSizeM = FVector2D(Length, Depth);
    D->Regions.Reset();
    TestTrue(TEXT("No outlines is valid"), D->ScatterBoundary(*P, FlatGround, None, Error));
    TestTrue(TEXT("No outlines places nothing"), None.IsEmpty());
    return true;
}
#endif
