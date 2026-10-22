// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "RangeTestMap.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"

namespace
{
    TAutoConsoleVariable<FString> TestMap(TEXT("uopengolf.TestMap"), TEXT("/Game/UOpenGolf/Maps/UOpenGolf"),
        TEXT("Map used by coastal terrain, visual and obstacle-contact tests."));
    TAutoConsoleVariable<FString> CaptureLabel(TEXT("uopengolf.CoastalCaptureLabel"), TEXT(""),
        TEXT("Folder label for coastal visual captures; empty uses the command line or Review."));
} // namespace

FString GetRangeTestMap()
{
    return TestMap.GetValueOnGameThread();
}

bool FindRangeBunkerCenter(const UOpenGolfTerrainDefinition& Definition, FVector2D& Center)
{
    for (const auto& Region : Definition.Regions)
        if (Region.Surface && Region.Surface->Surface == EOpenFairwaySurface::Sand && !Region.OutlineM.IsEmpty())
        {
            Center = FVector2D::ZeroVector;
            for (const FVector2D Point : Region.OutlineM)
                Center += Point;
            Center /= Region.OutlineM.Num();
            return UOpenGolfTerrainDefinition::Contains(Region.OutlineM, Center);
        }
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeBunkerProbeTest, "UOpenGolf.Range.BunkerProbe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangeBunkerProbeTest::RunTest(const FString& Parameters)
{
    auto* Definition = NewObject<UOpenGolfTerrainDefinition>();
    FVector2D Center;
    TestFalse(TEXT("A missing bunker has no probe"), FindRangeBunkerCenter(*Definition, Center));
    auto* Sand = NewObject<UOpenGolfSurfacePreset>();
    Sand->Surface = EOpenFairwaySurface::Sand;
    FOpenGolfTerrainRegion Bunker;
    Bunker.Name = TEXT("Bunker");
    Bunker.Surface = Sand;
    Bunker.OutlineM = {{100, -5}, {116, -5}, {116, 5}, {100, 5}};
    Definition->Regions.Add(Bunker);
    TestTrue(TEXT("A moved bunker has a probe"), FindRangeBunkerCenter(*Definition, Center));
    TestTrue(TEXT("The probe follows the authored outline"), Center.Equals(FVector2D(108, 0), 0.0001));
    for (auto& Point : Definition->Regions[0].OutlineM)
        Point += FVector2D(38, 28);
    TestTrue(TEXT("The original layout is also supported"), FindRangeBunkerCenter(*Definition, Center) && Center.Equals(FVector2D(146, 28), 0.0001));
    return true;
}
#endif
