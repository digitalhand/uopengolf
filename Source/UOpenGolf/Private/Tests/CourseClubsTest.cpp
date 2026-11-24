// SPDX-License-Identifier: MIT
// Club groups, PGA Tour carries and the club a course round picks: pure functions, no world.
#if WITH_DEV_AUTOMATION_TESTS
#include "Course/CourseClubs.h"
#include "Misc/AutomationTest.h"
#include "Play/OpenGolfLieRules.h"

namespace CourseClubsTestDetail
{
    constexpr float MetresPerYard = 0.9144f;

    CourseClubs::FAdvice Advice(float Yards, EOpenFairwaySurface Surface, bool bOnTee = false)
    {
        CourseClubs::FAdvice Next;
        Next.DistanceToCupM = Yards * MetresPerYard;
        Next.Surface = Surface;
        Next.bOnTee = bOnTee;
        Next.Rules = GetDefault<UOpenGolfLieRules>();
        return Next;
    }
} // namespace CourseClubsTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseClubsTest, "UOpenGolf.Course.Clubs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourseClubsTest::RunTest(const FString&)
{
    using namespace CourseClubsTestDetail;
    using namespace CourseClubs;
    using EClub = ELaunchMonitorClub;
    using ESurface = EOpenFairwaySurface;

    TestEqual(TEXT("The driver is a wood"), Group(EClub::Driver), EOpenGolfClubGroup::Wood);
    TestEqual(TEXT("The 7 wood is a wood"), Group(EClub::Wood7), EOpenGolfClubGroup::Wood);
    TestEqual(TEXT("The 4 iron is an iron"), Group(EClub::Iron4), EOpenGolfClubGroup::Iron);
    TestEqual(TEXT("The 9 iron is an iron"), Group(EClub::Iron9), EOpenGolfClubGroup::Iron);
    TestEqual(TEXT("The pitching wedge is a wedge"), Group(EClub::PitchingWedge), EOpenGolfClubGroup::Wedge);
    TestEqual(TEXT("The lob wedge is a wedge"), Group(EClub::LobWedge), EOpenGolfClubGroup::Wedge);
    TestEqual(TEXT("The putter is the putter"), Group(EClub::Putter), EOpenGolfClubGroup::Putter);

    TestEqual(TEXT("A PGA Tour driver carries 275 yd"), PgaCarryM(EClub::Driver), 275.0f * MetresPerYard, 0.01f);
    TestEqual(TEXT("A PGA Tour 7 iron carries 172 yd"), PgaCarryM(EClub::Iron7), 172.0f * MetresPerYard, 0.01f);
    TestEqual(TEXT("The putter has no carry"), PgaCarryM(EClub::Putter), 0.0f);
    for (int32 Index = 1; Index < static_cast<int32>(EClub::Putter); ++Index)
        TestTrue(FString::Printf(TEXT("Club %d carries shorter than the club before it"), Index),
            PgaCarryM(static_cast<EClub>(Index)) < PgaCarryM(static_cast<EClub>(Index - 1)));

    TestEqual(TEXT("The putter on the green"), Recommend(Advice(10, ESurface::Green)), EClub::Putter);
    TestEqual(TEXT("The putter from the fringe"), Recommend(Advice(15, ESurface::Fringe)), EClub::Putter);
    TestEqual(TEXT("The sand wedge from a greenside bunker"), Recommend(Advice(20, ESurface::Sand)), EClub::SandWedge);
    TestEqual(TEXT("A 7 iron's distance from the fairway"), Recommend(Advice(172, ESurface::Fairway)), EClub::Iron7);
    TestEqual(TEXT("The rough takes a longer club"), Recommend(Advice(172, ESurface::Rough)), EClub::Iron6);
    TestEqual(TEXT("A 9 iron from the fairway"), Recommend(Advice(150, ESurface::Fairway)), EClub::Iron9);
    TestEqual(TEXT("A fairway bunker takes more club"), Recommend(Advice(150, ESurface::Sand)), EClub::Iron7);
    TestEqual(TEXT("Two equally near clubs: the longer"), Recommend(Advice(166, ESurface::Fairway)), EClub::Iron7);
    TestEqual(TEXT("No driver off the fairway"), Recommend(Advice(300, ESurface::Fairway)), EClub::Wood3);
    TestEqual(TEXT("The driver from the tee"), Recommend(Advice(300, ESurface::Fairway, true)), EClub::Driver);
    TestEqual(TEXT("The driver on a long hole's tee"), Recommend(Advice(400, ESurface::Fairway, true)), EClub::Driver);
    TestEqual(TEXT("A short par 3 from the tee"), Recommend(Advice(148, ESurface::Fairway, true)), EClub::Iron9);
    TestEqual(TEXT("The tee plays clean whatever is beneath it"), Recommend(Advice(172, ESurface::Rough, true)), EClub::Iron7);
    TestEqual(TEXT("A short pitch takes the lob wedge"), Recommend(Advice(40, ESurface::Fairway)), EClub::LobWedge);

    FAdvice Clean = Advice(172, ESurface::Rough);
    Clean.Rules = nullptr;
    TestEqual(TEXT("Without lie rules the rough plays clean"), Recommend(Clean), EClub::Iron7);
    return true;
}

#endif
