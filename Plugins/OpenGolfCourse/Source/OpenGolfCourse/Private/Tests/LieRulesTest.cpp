// SPDX-License-Identifier: MIT
// Lie rules: the standard lie penalties, how a course looks one up, and what a penalty does to a measured launch.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Play/OpenGolfLieRules.h"
#include "Play/OpenGolfPlayRules.h"

namespace LieRulesTestDetail
{
    /** Well outside the short game. */
    constexpr float ApproachM = 150.0f;
    /** Inside 30 yd of the cup. */
    constexpr float GreensideM = 15.0f;

    /** A measured 7 iron with every value present except total spin. */
    FOpenFairwayLaunch IronShot()
    {
        FOpenFairwayLaunch Launch;
        Launch.BallSpeedMph = 120.0f;
        Launch.VerticalLaunchDeg = 16.3f;
        Launch.HorizontalLaunchDeg = -1.5f;
        Launch.bHasBackSpin = true;
        Launch.BackSpinRpm = 7000.0f;
        Launch.bHasSideSpin = true;
        Launch.SideSpinRpm = -400.0f;
        Launch.bHasSpinAxis = true;
        Launch.SpinAxisDeg = -3.0f;
        return Launch;
    }

    bool IsCut(const FOpenGolfLieCut& Cut, float BallSpeedPct, float SpinPct)
    {
        return FMath::IsNearlyEqual(Cut.BallSpeedPct, BallSpeedPct) && FMath::IsNearlyEqual(Cut.SpinPct, SpinPct);
    }

    bool SameLaunch(const FOpenFairwayLaunch& A, const FOpenFairwayLaunch& B)
    {
        return A.BallSpeedMph == B.BallSpeedMph && A.VerticalLaunchDeg == B.VerticalLaunchDeg && A.HorizontalLaunchDeg == B.HorizontalLaunchDeg
            && A.bHasBackSpin == B.bHasBackSpin && A.BackSpinRpm == B.BackSpinRpm && A.bHasSideSpin == B.bHasSideSpin && A.SideSpinRpm == B.SideSpinRpm
            && A.bHasTotalSpin == B.bHasTotalSpin && A.TotalSpinRpm == B.TotalSpinRpm && A.bHasSpinAxis == B.bHasSpinAxis && A.SpinAxisDeg == B.SpinAxisDeg;
    }
} // namespace LieRulesTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfLieRulesTest, "UOpenGolf.Course.Play.LieRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfLieRulesTest::RunTest(const FString& Parameters)
{
    using namespace LieRulesTestDetail;
    using ESurface = EOpenFairwaySurface;
    using EGroup = EOpenGolfClubGroup;
    const UOpenGolfLieRules& Standard = *GetDefault<UOpenGolfLieRules>();

    for (const ESurface Surface : {ESurface::Fairway, ESurface::FairwaySoft, ESurface::Firm, ESurface::Fringe, ESurface::Green})
        for (const EGroup Group : {EGroup::Wood, EGroup::Iron, EGroup::Wedge, EGroup::Putter})
            TestTrue(TEXT("A clean lie has no penalty"), Standard.CutFor(Surface, ApproachM, Group).IsNone());

    TestTrue(TEXT("Woods from the rough"), IsCut(Standard.CutFor(ESurface::Rough, ApproachM, EGroup::Wood), 10, 20));
    TestTrue(TEXT("Irons from the rough"), IsCut(Standard.CutFor(ESurface::Rough, ApproachM, EGroup::Iron), 8, 25));
    TestTrue(TEXT("Wedges from the rough lose the most spin"), IsCut(Standard.CutFor(ESurface::Rough, ApproachM, EGroup::Wedge), 6, 35));
    TestTrue(TEXT("Rough plays the same beside the green"), IsCut(Standard.CutFor(ESurface::Rough, GreensideM, EGroup::Iron), 8, 25));

    TestTrue(TEXT("Woods from a fairway bunker"), IsCut(Standard.CutFor(ESurface::Sand, ApproachM, EGroup::Wood), 15, 25));
    TestTrue(TEXT("Irons from a fairway bunker"), IsCut(Standard.CutFor(ESurface::Sand, ApproachM, EGroup::Iron), 10, 25));
    TestTrue(TEXT("Wedges from a fairway bunker"), IsCut(Standard.CutFor(ESurface::Sand, ApproachM, EGroup::Wedge), 8, 25));
    TestTrue(TEXT("A greenside bunker is an explosion shot"), IsCut(Standard.CutFor(ESurface::Sand, GreensideM, EGroup::Wedge), 40, 30));
    TestTrue(TEXT("Any club explodes from a greenside bunker"), IsCut(Standard.CutFor(ESurface::Sand, GreensideM, EGroup::Iron), 40, 30));
    // Greenside means the short game, which ends at 30 yd from the cup (OpenGolfPlay::IsShortGame).
    TestTrue(TEXT("Just inside 30 yd is greenside"),
        IsCut(Standard.CutFor(ESurface::Sand, OpenGolfPlay::ShortGameDistanceM - 0.01f, EGroup::Wedge), 40, 30));
    TestTrue(TEXT("30 yd out is a fairway bunker"), IsCut(Standard.CutFor(ESurface::Sand, OpenGolfPlay::ShortGameDistanceM, EGroup::Wedge), 8, 25));

    TestTrue(TEXT("The putter is never penalised in the rough"), Standard.CutFor(ESurface::Rough, ApproachM, EGroup::Putter).IsNone());
    TestTrue(TEXT("The putter is never penalised in sand"), Standard.CutFor(ESurface::Sand, GreensideM, EGroup::Putter).IsNone());

    // A course's own rules start from the standard; a surface left out has no penalty, and a near-green row applies
    // only in the short game.
    UOpenGolfLieRules* Course = NewObject<UOpenGolfLieRules>();
    TestTrue(TEXT("A new asset starts from the standard"), IsCut(Course->CutFor(ESurface::Rough, ApproachM, EGroup::Iron), 8, 25));
    Course->Lies.Remove(ESurface::Rough);
    TestTrue(TEXT("A surface left out has no penalty"), Course->CutFor(ESurface::Rough, ApproachM, EGroup::Iron).IsNone());
    FOpenGolfLiePenalty Collar;
    Collar.Irons = FOpenGolfLieCut(20, 50);
    Course->NearGreenLies.Add(ESurface::Rough, Collar);
    TestTrue(TEXT("A near-green row applies beside the green"), IsCut(Course->CutFor(ESurface::Rough, GreensideM, EGroup::Iron), 20, 50));
    TestTrue(TEXT("A near-green row does not apply farther out"), Course->CutFor(ESurface::Rough, ApproachM, EGroup::Iron).IsNone());
    TestTrue(TEXT("The standard is unchanged by a course's asset"), IsCut(Standard.CutFor(ESurface::Rough, ApproachM, EGroup::Iron), 8, 25));

    // The penalty comes off the measured ball speed and every spin present; direction and launch angles stay measured.
    const FOpenFairwayLaunch Measured = IronShot();
    const FOpenFairwayLaunch Penalised = OpenGolfPlay::ApplyLieCut(Measured, FOpenGolfLieCut(8, 25));
    TestEqual(TEXT("Ball speed loses 8%"), Penalised.BallSpeedMph, 110.4f, 1e-3f);
    TestEqual(TEXT("Back spin loses 25%"), Penalised.BackSpinRpm, 5250.0f, 1e-2f);
    TestEqual(TEXT("Side spin loses 25%"), Penalised.SideSpinRpm, -300.0f, 1e-2f);
    TestFalse(TEXT("No total spin is invented"), Penalised.bHasTotalSpin);
    TestEqual(TEXT("A missing spin stays empty"), Penalised.TotalSpinRpm, 0.0f);
    TestEqual(TEXT("Launch angle stays measured"), Penalised.VerticalLaunchDeg, Measured.VerticalLaunchDeg);
    TestEqual(TEXT("Direction stays measured"), Penalised.HorizontalLaunchDeg, Measured.HorizontalLaunchDeg);
    TestEqual(TEXT("Spin axis stays measured"), Penalised.SpinAxisDeg, Measured.SpinAxisDeg);
    TestTrue(TEXT("The measured flags are kept"), Penalised.bHasBackSpin && Penalised.bHasSideSpin && Penalised.bHasSpinAxis);
    TestEqual(TEXT("The measured launch is left as it was"), Measured.BallSpeedMph, 120.0f);
    TestTrue(TEXT("No penalty leaves the launch as measured"), SameLaunch(OpenGolfPlay::ApplyLieCut(Measured, FOpenGolfLieCut()), Measured));

    FOpenFairwayLaunch TotalOnly;
    TotalOnly.BallSpeedMph = 100.0f;
    TotalOnly.VerticalLaunchDeg = 20.0f;
    TotalOnly.bHasTotalSpin = true;
    TotalOnly.TotalSpinRpm = 8000.0f;
    TestEqual(TEXT("A total spin loses its share"), OpenGolfPlay::ApplyLieCut(TotalOnly, FOpenGolfLieCut(40, 30)).TotalSpinRpm, 5600.0f, 1e-2f);
    TestEqual(TEXT("An authored cut beyond the limit is held at it"),
        OpenGolfPlay::ApplyLieCut(TotalOnly, FOpenGolfLieCut(150, 0)).BallSpeedMph, 100.0f * (1.0f - FOpenGolfLieCut::MaxPct / 100.0f), 1e-3f);

    // The minimap header: the speed cut beside the lie, the spin cut on the right, nothing for a clean lie.
    TestEqual(TEXT("Speed cut label"), OpenGolfPlay::LieCutLabel(FOpenGolfLieCut(8, 25)), FString(TEXT("−8%")));
    TestEqual(TEXT("Spin cut label"), OpenGolfPlay::SpinCutLabel(FOpenGolfLieCut(8, 25)), FString(TEXT("Spin −25%")));
    TestEqual(TEXT("Whole percent"), OpenGolfPlay::LieCutLabel(FOpenGolfLieCut(7.6f, 0)), FString(TEXT("−8%")));
    TestTrue(TEXT("No speed cut, no label"), OpenGolfPlay::LieCutLabel(FOpenGolfLieCut()).IsEmpty());
    TestTrue(TEXT("No spin cut, no label"), OpenGolfPlay::SpinCutLabel(FOpenGolfLieCut(10, 0)).IsEmpty());
    return true;
}

#endif
