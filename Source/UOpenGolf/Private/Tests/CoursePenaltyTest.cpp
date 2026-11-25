// SPDX-License-Identifier: MIT
// Where a course round's ball comes to rest decides its penalty: out of bounds, in a lake, or none.
#if WITH_DEV_AUTOMATION_TESTS
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoursePenaltyTest, "UOpenGolf.Course.Play.Penalties",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCoursePenaltyTest::RunTest(const FString&)
{
    auto* Course = NewObject<UGolfCourseDefinition>();
    TestEqual(TEXT("A course without outlines has no penalties"), Course->PenaltyAt(FVector(1e6, 0, 0)), EGolfPenalty::None);

    Course->InBoundsCm = {FVector2D(0, 0), FVector2D(40000, 0), FVector2D(40000, 20000), FVector2D(0, 20000)};
    FGolfWaterArea Lake;
    Lake.OutlineCm = {FVector2D(10000, 5000), FVector2D(20000, 5000), FVector2D(20000, 15000), FVector2D(10000, 15000)};
    Course->Water = {Lake};

    TestEqual(TEXT("In bounds and dry"), Course->PenaltyAt(FVector(5000, 5000, 120)), EGolfPenalty::None);
    TestEqual(TEXT("Past the boundary"), Course->PenaltyAt(FVector(-100, 5000, 0)), EGolfPenalty::OutOfBounds);
    TestEqual(TEXT("Resting in the lake"), Course->PenaltyAt(FVector(15000, 10000, -80)), EGolfPenalty::Water);
    TestEqual(TEXT("Carried the lake and landed beyond it"), Course->PenaltyAt(FVector(25000, 10000, 0)), EGolfPenalty::None);
    TestEqual(TEXT("A ball far below the ground is judged by where it is, not its height"),
        Course->PenaltyAt(FVector(5000, 5000, -1e6)), EGolfPenalty::None);
    return true;
}
#endif
