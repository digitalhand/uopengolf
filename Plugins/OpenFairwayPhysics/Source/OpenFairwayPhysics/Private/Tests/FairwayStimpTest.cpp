// SPDX-License-Identifier: MIT
// Fairway speed as a stimp reading: the fitted fairway is the standard, other readings measure on a stimpmeter, and
// only the fairway changes.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairwayBall.h"
#include "PhysicsTestUtils.h"
#include "UObject/Package.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayFairwayStimpTest, "UOpenGolf.Physics.FairwayStimp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayFairwayStimpTest::RunTest(const FString& Parameters)
{
    using namespace OpenFairwayTest;
    const FResolvedShotParams Shot;
    const FBallPhysicsProfile Default;
    const float Fitted = SurfaceCatalog::Get(ESurface::Fairway).RollingFriction;

    TestEqual(TEXT("The profile starts at the standard fairway"), Default.FairwayStimpFeet, FairwaySpeed::StandardStimpFeet);
    TestTrue(TEXT("The standard is the fitted fairway, unchanged"), Shot.ResolveSurface(ESurface::Fairway, Default).RollingFriction == Fitted);
    TestTrue(TEXT("The standard reading solves to the fitted value exactly"), FairwaySpeed::RollingFriction(FairwaySpeed::StandardStimpFeet) == Fitted);

    TestEqual(TEXT("A fast reading is clamped"), FairwaySpeed::Clamp(30.0f), FairwaySpeed::MaxStimpFeet);
    TestEqual(TEXT("A slow reading is clamped"), FairwaySpeed::Clamp(1.0f), FairwaySpeed::MinStimpFeet);
    TestEqual(TEXT("No reading plays the standard"), FairwaySpeed::Clamp(0.0f), FairwaySpeed::StandardStimpFeet);
    TestEqual(TEXT("A broken reading plays the standard"), FairwaySpeed::Clamp(NAN), FairwaySpeed::StandardStimpFeet);

    float PreviousRoll = 0.0f;
    for (const float Stimp : {6.0f, FairwaySpeed::StandardStimpFeet, 8.0f, 9.0f, 10.0f})
    {
        FBallPhysicsProfile Selected;
        Selected.FairwayStimpFeet = Stimp;
        const float Roll = RollMeters(Shot.ResolveSurface(ESurface::Fairway, Selected), GreenSpeed::StimpmeterSpeedMps);
        TestEqual(FString::Printf(TEXT("Fairway stimp %.1f measures within 0.1 m"), Stimp), Roll, Stimp * MetersPerFoot, 0.1f);
        TestTrue(FString::Printf(TEXT("Fairway stimp %.1f rolls farther than the slower one"), Stimp), Roll > PreviousRoll);
        PreviousRoll = Roll;
    }

    // Only the fairway follows the setting: every other surface, and the fairway's grass drag, stay as they were.
    FBallPhysicsProfile Fast;
    Fast.FairwayStimpFeet = 10.0f;
    for (const ESurface Surface : {ESurface::FairwaySoft, ESurface::Rough, ESurface::Firm, ESurface::Green, ESurface::Sand, ESurface::Fringe})
    {
        TestTrue(FString::Printf(TEXT("%s keeps its rolling friction"), SurfaceCatalog::GetName(Surface)),
            Shot.ResolveSurface(Surface, Fast).RollingFriction == Shot.ResolveSurface(Surface, Default).RollingFriction);
    }
    TestTrue(TEXT("The fairway keeps its grass drag"),
        Shot.ResolveSurface(ESurface::Fairway, Fast).GrassViscosity == Shot.ResolveSurface(ESurface::Fairway, Default).GrassViscosity &&
            Shot.ResolveSurface(ESurface::Fairway, Fast).GrassFormDrag == Shot.ResolveSurface(ESurface::Fairway, Default).GrassFormDrag);

    // The ball's own setting reaches the shot, as its green stimp does, and leaves its profile alone.
    AOpenFairwayBall* Ball = NewObject<AOpenFairwayBall>(GetTransientPackage());
    TestEqual(TEXT("The ball starts at the standard fairway"), Ball->FairwayStimpFeet, FairwaySpeed::StandardStimpFeet);
    FOpenFairwayLaunch Chip;
    Chip.BallSpeedMph = 20.0f;
    Chip.VerticalLaunchDeg = 2.0f;
    auto TotalYd = [Ball, &Chip](float StimpFeet, EOpenFairwaySurface Surface)
    {
        Ball->FairwayStimpFeet = StimpFeet;
        return Ball->PredictShot(Chip, Surface).TotalM * YardsPerMeter;
    };
    const float Standard = TotalYd(FairwaySpeed::StandardStimpFeet, EOpenFairwaySurface::Fairway);
    const float Headless = ShotSimulator::Simulate(AOpenFairwayBall::ToLaunchData(Chip), ESurface::Fairway).TotalM * YardsPerMeter;
    TestEqual(TEXT("The standard fairway predicts the default profile's chip"), Standard, Headless, 1e-4f);
    TestTrue(TEXT("A faster fairway rolls the chip farther"), TotalYd(10.0f, EOpenFairwaySurface::Fairway) > Standard);
    TestTrue(TEXT("A slower fairway rolls the chip shorter"), TotalYd(6.0f, EOpenFairwaySurface::Fairway) < Standard);
    TestEqual(TEXT("Fairway speed does not change the green"), TotalYd(10.0f, EOpenFairwaySurface::Green),
        TotalYd(FairwaySpeed::StandardStimpFeet, EOpenFairwaySurface::Green), 1e-4f);
    TestEqual(TEXT("Prediction leaves the ball profile alone"), Ball->Profile.FairwayStimpFeet, FairwaySpeed::StandardStimpFeet);
    return true;
}

#endif
