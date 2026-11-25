// SPDX-License-Identifier: MIT
// A tee set is found by name on every hole, as the round plays it, so the Courses card shows the length played.
#if WITH_DEV_AUTOMATION_TESTS
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"

namespace CourseTeeSetTestDetail
{
    FGolfCourseTee Tee(const TCHAR* SetName, int32 Yards)
    {
        FGolfCourseTee Result;
        Result.SetName = SetName;
        Result.PublishedYards = Yards;
        return Result;
    }
} // namespace CourseTeeSetTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseTeeSetTest, "UOpenGolf.Course.Play.TeeSets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCourseTeeSetTest::RunTest(const FString&)
{
    using CourseTeeSetTestDetail::Tee;
    auto* Course = NewObject<UGolfCourseDefinition>();
    FGolfCourseHole First, Reordered, Short;
    First.Tees = {Tee(TEXT("Blue"), 400), Tee(TEXT("Red"), 320)};
    Reordered.Tees = {Tee(TEXT("Red"), 150), Tee(TEXT("Blue"), 190)};
    Short.Tees = {Tee(TEXT("Blue"), 510)};
    Course->Layout = {First, Reordered, Short};

    TestEqual(TEXT("A set is found by name, wherever the hole lists it"), Reordered.FindTee(TEXT("Blue"))->PublishedYards, 190);
    TestEqual(TEXT("A hole without the set plays its first tee"), Short.FindTee(TEXT("Red"))->PublishedYards, 510);
    TestNull(TEXT("A hole without tees has none to play"), FGolfCourseHole().FindTee(TEXT("Red")));
    TestEqual(TEXT("Red as played: 320 + 150 + 510"), Course->TeeSetYards(TEXT("Red")), 980);
    TestEqual(TEXT("Blue as played: 400 + 190 + 510"), Course->TeeSetYards(TEXT("Blue")), 1100);
    return true;
}
#endif
