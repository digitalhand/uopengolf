// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "UObject/Package.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGreenStimpTest, "UOpenGolf.Physics.GreenStimp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayGreenStimpTest::RunTest(const FString& Parameters)
{
    AOpenFairwayBall* Ball = NewObject<AOpenFairwayBall>(GetTransientPackage());
    TestEqual(TEXT("Default green is 10 ft on the stimpmeter"), Ball->GreenStimpFeet, GreenSpeed::DefaultStimpFeet);

    // The fixture putt: rolling from the face with no measured spin.
    FOpenFairwayLaunch Putt;
    Putt.BallSpeedMph = 8.0f;
    Putt.VerticalLaunchDeg = 1.5f;
    auto TotalYd = [Ball, &Putt](float StimpFeet, EOpenFairwaySurface Surface)
    {
        Ball->GreenStimpFeet = StimpFeet;
        return Ball->PredictShot(Putt, Surface).TotalM * YardsPerMeter;
    };

    const float Default = TotalYd(GreenSpeed::DefaultStimpFeet, EOpenFairwaySurface::Green);
    const float Headless = ShotSimulator::Simulate(AOpenFairwayBall::ToLaunchData(Putt), ESurface::Green).TotalM * YardsPerMeter;
    TestEqual(TEXT("Default stimp predicts the default profile's putt"), Default, Headless, 1e-4f);

    const float Fast = TotalYd(13.0f, EOpenFairwaySurface::Green);
    const float Slow = TotalYd(8.0f, EOpenFairwaySurface::Green);
    AddInfo(FString::Printf(TEXT("8 mph putt: stimp 8 %.2f yd, 10 %.2f yd, 13 %.2f yd"), Slow, Default, Fast));
    TestTrue(TEXT("A faster green rolls the putt farther"), Fast > Default);
    TestTrue(TEXT("A slower green rolls the putt shorter"), Slow < Default);
    TestEqual(TEXT("Stimp above the range plays as the fastest green"), TotalYd(30.0f, EOpenFairwaySurface::Green),
        TotalYd(GreenSpeed::MaxStimpFeet, EOpenFairwaySurface::Green), 1e-4f);
    TestEqual(TEXT("Stimp does not change the fairway"), TotalYd(13.0f, EOpenFairwaySurface::Fairway),
        TotalYd(GreenSpeed::DefaultStimpFeet, EOpenFairwaySurface::Fairway), 1e-4f);
    TestEqual(TEXT("Prediction leaves the ball profile alone"), Ball->Profile.GreenStimpFeet, GreenSpeed::DefaultStimpFeet);
    return true;
}

#endif
