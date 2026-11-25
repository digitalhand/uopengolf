// SPDX-License-Identifier: MIT
// Distance from play for the hole map: the definition's own answer up to a cap, from a copy of the regions that can
// matter in one area, so worker threads can read it.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenGolfPlayDistance.h"
#include "OpenGolfTerrain.h"

namespace TerrainPlayDistanceTestDetail
{
    FOpenGolfTerrainRegion Region(UOpenGolfSurfacePreset* Surface, const TArray<FVector2D>& OutlineM)
    {
        FOpenGolfTerrainRegion Result;
        Result.Surface = Surface;
        Result.OutlineM = OutlineM;
        return Result;
    }

    TArray<FVector2D> Box(double X0, double Y0, double X1, double Y1)
    {
        return {FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1)};
    }

    UOpenGolfSurfacePreset* Preset(EOpenFairwaySurface Surface)
    {
        auto* Result = NewObject<UOpenGolfSurfacePreset>();
        Result->Surface = Surface;
        return Result;
    }
} // namespace TerrainPlayDistanceTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainPlayDistanceTest, "UOpenGolf.Course.Terrain.PlayDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTerrainPlayDistanceTest::RunTest(const FString&)
{
    using namespace TerrainPlayDistanceTestDetail;
    constexpr double Cap = 40;
    const FBox2D Area(FVector2D(-100, -100), FVector2D(300, 200));
    auto* Definition = NewObject<UOpenGolfTerrainDefinition>();
    UOpenGolfSurfacePreset* Rough = Preset(EOpenFairwaySurface::Rough);
    UOpenGolfSurfacePreset* Fairway = Preset(EOpenFairwaySurface::Fairway);
    UOpenGolfSurfacePreset* Green = Preset(EOpenFairwaySurface::Green);
    TestEqual(TEXT("Without play regions every point is at the cap"), FOpenGolfPlayDistance(*Definition, Area, Cap).At(FVector2D(5, 5)), Cap);
    TestFalse(TEXT("...and the terrain has no play to measure from"), FOpenGolfPlayDistance(*Definition, Area, Cap).HasPlayRegions());

    const TArray<FVector2D> GreenOutline = {FVector2D(210, -10), FVector2D(230, 0), FVector2D(220, 15), FVector2D(205, 5)};
    Definition->Regions = {
        Region(Rough, Box(-500, -500, 500, 500)),        // Rough, like a course's play area, is never play.
        Region(Fairway, Box(0, -20, 200, 20)),
        Region(Green, GreenOutline),
        Region(Fairway, Box(1000, 1000, 1100, 1100)),    // Farther than the cap from the whole area.
    };
    const FOpenGolfPlayDistance Distance(*Definition, Area, Cap);
    TestEqual(TEXT("Only the play regions that can matter are kept"), Distance.NumRegions(), 2);
    const FOpenGolfPlayDistance Beyond(*Definition, FBox2D(FVector2D(5000, 5000), FVector2D(5100, 5100)), Cap);
    TestTrue(TEXT("An area far from play still knows the terrain has play"), Beyond.NumRegions() == 0 && Beyond.HasPlayRegions());

    int32 Mismatches = 0;
    for (double X = Area.Min.X; X <= Area.Max.X; X += 2.5)
    {
        for (double Y = Area.Min.Y; Y <= Area.Max.Y; Y += 2.5)
        {
            const FVector2D P(X, Y);
            Mismatches += !FMath::IsNearlyEqual(Distance.At(P), FMath::Min(Definition->DistanceFromPlay(P), Cap), 1e-9);
            const double Reference = UOpenGolfTerrainDefinition::Contains(GreenOutline, P) ? 0.0 : UOpenGolfTerrainDefinition::DistanceToOutline(GreenOutline, P);
            Mismatches += UOpenGolfTerrainDefinition::DistanceFromOutline(GreenOutline, P) != Reference;
        }
    }
    TestEqual(TEXT("Across the area: the capped distance from play, and one pass agrees with Contains and DistanceToOutline"), Mismatches, 0);
    TestEqual(TEXT("On a fairway edge"), Distance.At(FVector2D(100, 20)), 0.0);
    TestEqual(TEXT("Inside the green"), Distance.At(FVector2D(218, 3)), 0.0);
    TestEqual(TEXT("Beside the fairway"), Distance.At(FVector2D(100, 30)), 10.0, 1e-9);
    TestEqual(TEXT("Well away from play"), Distance.At(FVector2D(-100, 150)), Cap);
    TestEqual(TEXT("A line has no inside"), UOpenGolfTerrainDefinition::DistanceFromOutline({FVector2D(0, 0), FVector2D(10, 0)}, FVector2D(5, 3)), 3.0, 1e-9);
    return true;
}
#endif
