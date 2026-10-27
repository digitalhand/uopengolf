// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfTerrainBuilder.h"

namespace VegetationRebuildTestDetail
{
    /** The course grass tag set by the vegetation builder. */
    const FName CourseGrassTag(TEXT("OpenGolf.Terrain.CourseGrass"));
    const FName PendingCourseGrassTag(TEXT("OpenGolf.Terrain.PendingCourseGrass"));

    AActor* MakeActor(std::initializer_list<FName> Tags)
    {
        AActor* Actor = NewObject<AActor>(GetTransientPackage());
        Actor->Tags.Append(Tags.begin(), static_cast<int32>(Tags.size()));
        return Actor;
    }
} // namespace VegetationRebuildTestDetail

// A vegetation rebuild replaces trees, ground cover and understory hosts, but never the course
// grass layer, which only RebuildCourseGrass places and nothing else recreates.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVegetationRebuildTest, "UOpenGolf.Course.VegetationRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVegetationRebuildTest::RunTest(const FString& Parameters)
{
    namespace Detail = VegetationRebuildTestDetail;
    using Area = AOpenGolfTerrainArea;
    auto Replaced = [](const AActor* Actor) { return UOpenGolfTerrainBuilder::IsReplacedByVegetationRebuild(*Actor); };

    TestTrue(TEXT("Ground cover is replaced"), Replaced(Detail::MakeActor({Area::GeneratedTag, Area::VegetationTag})));
    TestTrue(TEXT("Trees are replaced"), Replaced(Detail::MakeActor({Area::GeneratedTag, Area::TreesTag})));
    TestTrue(TEXT("Trunk colliders are replaced"), Replaced(Detail::MakeActor({Area::GeneratedTag, Area::TreeCollidersTag})));
    TestTrue(TEXT("Canopy volumes are replaced"), Replaced(Detail::MakeActor({Area::GeneratedTag, Area::TreeCanopyTag})));
    TestFalse(TEXT("Baked course grass is kept"), Replaced(Detail::MakeActor({Area::GeneratedTag, Area::VegetationTag, Detail::CourseGrassTag})));
    TestFalse(TEXT("Pending course grass is kept"),
        Replaced(Detail::MakeActor({Area::GeneratedTag, Area::VegetationTag, Detail::CourseGrassTag, Detail::PendingCourseGrassTag})));
    TestFalse(TEXT("Terrain and layout are kept"), Replaced(Detail::MakeActor({Area::GeneratedTag})));
    return true;
}
#endif
